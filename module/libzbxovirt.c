
// Zabbix source headers
#include "sysinc.h"
#include "module.h"
#include "common.h"
#include "log.h"
#include "zbxjson.h"
#include "version.h"

#include <curl/curl.h>
#include <libconfig.h>

#define JSON_TAG_STATISTICS	"statistic"
#define JSON_PATH_STATISTICS_NAME	"$.name"
#define JSON_PATH_STATISTICS_VALUE	"$.values.value[0].datum"

#define MODULE_CFG	"/etc/zabbix/libzbxovirt.conf"
#define MODULE_NAME	"libzbxovirt.so"
#define ovirt_log(LEVEL, FMT, ...)	zabbix_log(LEVEL, "[%s;%s:%s:%d] " FMT, MODULE_NAME,  __FILE__, __FUNCTION__,  __LINE__, ##__VA_ARGS__) 

struct curl_fetch_st {
	char *payload;
	size_t size;
	struct zbx_json_parse json;
};

static int	item_timeout = 0;

config_t	cfg;
const char	*cfg_ovirt_user;
const char	*cfg_ovirt_pass;
const char	*cfg_ovirt_server;
const char	*cfg_ovirt_uri;

int zbx_module_ovirt_simple_stats(AGENT_REQUEST *request, AGENT_RESULT *result);
int zbx_module_ovirt_simple_data(AGENT_REQUEST *request, AGENT_RESULT *result);

int zbx_module_ovirt_host_stats(AGENT_REQUEST *request, AGENT_RESULT *result);
int zbx_module_ovirt_host_data(AGENT_REQUEST *request, AGENT_RESULT *result);

int zbx_module_ovirt_vm_stats(AGENT_REQUEST *request, AGENT_RESULT *result);
int zbx_module_ovirt_vm_data(AGENT_REQUEST *request, AGENT_RESULT *result);

int ovirt_config_init(config_t *cfg);
int ovirt_config_uninit(config_t *cfg);
int ovirt_config_get(config_t *cfgi, const char* name);

/* module SHOULD define internal functions as static and use a naming pattern different from Zabbix internal */
/* symbols (zbx_*) and loadable module API functions (zbx_module_*) to avoid conflicts                       */
static ZBX_METRIC keys[] =
/*	KEY			FLAG		FUNCTION	TEST PARAMETERS */
{
	{"ovirt.simple.stats", CF_HAVEPARAMS,	zbx_module_ovirt_simple_stats,	"vms/123"},
	{"ovirt.simple.data",	CF_HAVEPARAMS,	zbx_module_ovirt_simple_data,	"vms/123"},
	{"ovirt.host.data",	CF_HAVEPARAMS,	zbx_module_ovirt_host_data,	"123"},
	{"ovirt.host.stats",	CF_HAVEPARAMS,	zbx_module_ovirt_host_stats,	"123"},
	{"ovirt.vm.data",	CF_HAVEPARAMS,	zbx_module_ovirt_vm_data,	"123"},
	{"ovirt.vm.stats",	CF_HAVEPARAMS,	zbx_module_ovirt_vm_stats,	"123"},
	{NULL}
};

/******************************************************************************
 *                                                                            *
 * Function: zbx_module_api_version                                           *
 *                                                                            *
 * Purpose: returns version number of the module interface                    *
 *                                                                            *
 * Return value: ZBX_MODULE_API_VERSION - version of module.h module is       *
 *               compiled with, in order to load module successfully Zabbix   *
 *               MUST be compiled with the same version of this header file   *
 *                                                                            *
 ******************************************************************************/
int zbx_module_api_version(void) {
	return ZBX_MODULE_API_VERSION;
}

/******************************************************************************
 *                                                                            *
 * Function: zbx_module_item_timeout                                          *
 *                                                                            *
 * Purpose: set timeout value for processing of items                         *
 *                                                                            *
 * Parameters: timeout - timeout in seconds, 0 - no timeout set               *
 *                                                                            *
 ******************************************************************************/
void zbx_module_item_timeout(int timeout) {
	item_timeout = timeout;
}

/******************************************************************************
 *                                                                            *
 * Function: zbx_module_item_list                                             *
 *                                                                            *
 * Purpose: returns list of item keys supported by the module                 *
 *                                                                            *
 * Return value: list of item keys                                            *
 *                                                                            *
 ******************************************************************************/
ZBX_METRIC *zbx_module_item_list(void) {
	return keys;
}

/******************************************************************************
 *                                                                            *
 * Function: zbx_module_init                                                  *
 *                                                                            *
 * Purpose: the function is called on agent startup                           *
 *          It should be used to call any initialization routines             *
 *                                                                            *
 * Return value: ZBX_MODULE_OK - success                                      *
 *               ZBX_MODULE_FAIL - module initialization failed               *
 *                                                                            *
 * Comment: the module won't be loaded in case of ZBX_MODULE_FAIL             *
 *                                                                            *
 ******************************************************************************/
int zbx_module_init(void) {
	int ret = ZBX_MODULE_OK;
	ovirt_log(LOG_LEVEL_INFORMATION, "compile time %s %s",
		__DATE__, __TIME__);
	ovirt_log(LOG_LEVEL_INFORMATION, "starting agent module %s",
		PACKAGE_STRING);

	curl_global_init(CURL_GLOBAL_DEFAULT);
	ret = ovirt_config_init(&cfg);
	return ret;
}

/******************************************************************************
 *                                                                            *
 * Function: zbx_module_uninit                                                *
 *                                                                            *
 * Purpose: the function is called on agent shutdown                          *
 *          It should be used to cleanup used resources if there are any      *
 *                                                                            *
 * Return value: ZBX_MODULE_OK - success                                      *
 *               ZBX_MODULE_FAIL - function failed                            *
 *                                                                            *
 ******************************************************************************/
int zbx_module_uninit(void) {
	int ret = ZBX_MODULE_OK;
	curl_global_cleanup();
	ret = ovirt_config_uninit(&cfg);
	return ret;
}
/******************************************************************************
 *                                                                            *
 * Function: zbx_module_history_write_cbs                                     *
 *                                                                            *
 * Purpose: returns a set of module functions Zabbix will call to export      *
 *          different types of historical data                                *
 *                                                                            *
 * Return value: structure with callback function pointers (can be NULL if    *
 *               module is not interested in data of certain types)           *
 *                                                                            *
 ******************************************************************************/
ZBX_HISTORY_WRITE_CBS	zbx_module_history_write_cbs(void) {
	static ZBX_HISTORY_WRITE_CBS	callbacks = {
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
	};
	return callbacks;
}

int ovirt_config_init(config_t *cfg) {
	ovirt_log(LOG_LEVEL_INFORMATION, "loading configiration from %s", MODULE_CFG);
	config_init(cfg);
	if(! config_read_file(cfg, MODULE_CFG)) {
		ovirt_log(LOG_LEVEL_ERR, "error reafing config file %s:%d - %s",
			config_error_file(cfg), config_error_line(cfg), config_error_text(cfg));
		config_destroy(cfg);
		return(ZBX_MODULE_FAIL);
	}

	if(!config_lookup_string(cfg, "ovirt.engine.user", &cfg_ovirt_user)) {
		ovirt_log(LOG_LEVEL_ERR, "ovirt.engine.user not found");
		config_destroy(cfg);
		return(ZBX_MODULE_FAIL);
	}
	if(!config_lookup_string(cfg, "ovirt.engine.pass", &cfg_ovirt_pass)) {
		ovirt_log(LOG_LEVEL_ERR, "ovirt.engine.pass not found");
		config_destroy(cfg);
		return(ZBX_MODULE_FAIL);
	}
	if(!config_lookup_string(cfg, "ovirt.engine.server", &cfg_ovirt_server)) {
		ovirt_log(LOG_LEVEL_ERR, "ovirt.engine.server not found");
		config_destroy(cfg);
		return(ZBX_MODULE_FAIL);
	}
	if(!config_lookup_string(cfg, "ovirt.engine.uri", &cfg_ovirt_uri)) {
		ovirt_log(LOG_LEVEL_ERR, "ovirt.engine.uri not found");
		config_destroy(cfg);
		return(ZBX_MODULE_FAIL);
	}

	return ZBX_MODULE_OK;
}

int ovirt_config_uninit(config_t *cfg) {
	config_destroy(cfg);
	return ZBX_MODULE_OK;
}

/* callback for curl fetch */
size_t __curl_write_callback (void *contents, size_t size, size_t nmemb, void *userdata) {
	size_t realsize = size * nmemb;	/* calculate buffer size */
	struct curl_fetch_st *p = (struct curl_fetch_st *) userdata;   /* cast pointer to fetch struct */

	/* expand buffer */
	p->payload = (char *) realloc(p->payload, p->size + realsize + 1);
	/* check buffer */
	if (p->payload == NULL) {
		/* this isn't good */
		ovirt_log(LOG_LEVEL_ERR, "Failed to expand buffer in curl_callback");
		/* free buffer */
		free(p->payload);
		p->payload = NULL;
		p->size = 0;
		/* return */
		return 0;
	}

	/* copy contents to buffer */
	memcpy(&(p->payload[p->size]), contents, realsize);

        /* set new buffer size */
        p->size += realsize;

	/* ensure null termination */
	p->payload[p->size] = 0;

	/* return size */
        return realsize;
}

void __curl_perform_query(char* url, struct curl_fetch_st *response)  {
	CURL *curl = NULL;
	CURLcode res;
	ovirt_log(LOG_LEVEL_DEBUG, "performing query for %s",url);
	curl = curl_easy_init();
	if (curl) {
		struct curl_slist *headers = NULL;
		headers = curl_slist_append(headers, "Accept: application/json");
		headers = curl_slist_append(headers, "Content-Type: application/json");
		curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTP|CURLPROTO_HTTPS);
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
		curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
		curl_easy_setopt(curl, CURLOPT_USERNAME, cfg_ovirt_user);
		curl_easy_setopt(curl, CURLOPT_PASSWORD, cfg_ovirt_pass);
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, __curl_write_callback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)response);
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
		res = curl_easy_perform(curl);
		if(res != CURLE_OK) {
			ovirt_log(LOG_LEVEL_ERR, "curl_easy_perform failed(): %s",
				curl_easy_strerror(res));
		}
		curl_slist_free_all(headers);
	}
	/* always cleanup */
	curl_easy_cleanup(curl);

	if (response->payload) {
		int ret = zbx_json_open(response->payload, &(response->json));
		if (ret != SUCCEED) {
			ovirt_log(LOG_LEVEL_ERR, "Failed to parse json object");
			free(response->payload);
			response->payload = NULL;
			response->size = 0;
		}
	} else {
		ovirt_log(LOG_LEVEL_DEBUG, "did not received payload");
	}
}

int __get_data(char *url, AGENT_RESULT *result) {
	struct	curl_fetch_st response;
	int ret;

	ret = SYSINFO_RET_OK;
	response.payload = NULL;
	response.size = 0;

	__curl_perform_query(url, &response);

	if (!response.payload) {
		SET_STR_RESULT(result, strdup("incorrect data received"));
		ret = SYSINFO_RET_FAIL;
		goto exit;
	}

	SET_TEXT_RESULT(result, strdup(response.payload));
exit:
	free(response.payload);
	return ret;
}

int __get_statistics(char *url, AGENT_RESULT *result) {
	struct	curl_fetch_st response;
	struct zbx_json j;
	int ret = SYSINFO_RET_OK;

	// initialize variables
	response.payload = NULL;
	response.size = 0;
	zbx_json_init(&j, ZBX_JSON_STAT_BUF_LEN);

	__curl_perform_query(url, &response);

	if (!response.payload) {
		SET_STR_RESULT(result, strdup("incorrect data received"));
		ret = SYSINFO_RET_FAIL;
		goto exit;
	}

	struct zbx_json_parse jp_data, jp_row, jp_path;
	const char *p = NULL;

	if (FAIL == zbx_json_brackets_by_name(&(response.json), JSON_TAG_STATISTICS, &jp_data)) {
		ovirt_log(LOG_LEVEL_ERR, "unable to find '%s' array", JSON_TAG_STATISTICS); 
		ret = SYSINFO_RET_FAIL;
		SET_STR_RESULT(result, strdup("incorrect data received"));
		goto exit;
	}

	do {
		p = zbx_json_next(&jp_data, p);
		if (p == NULL)
			break;

		if (FAIL == zbx_json_brackets_open(p, &jp_row)) {
			SET_STR_RESULT(result, strdup("Cannot open value object in received JSON"));
			ret = SYSINFO_RET_FAIL;
			goto exit;
		}

		char	*stats_name = NULL;
		char	*stats_value = NULL;
		size_t	stats_alloc;

		if (FAIL == zbx_json_path_open(&jp_row, JSON_PATH_STATISTICS_NAME, &jp_path)) {
			SET_STR_RESULT(result, strdup(zbx_json_strerror()));
			ret = SYSINFO_RET_FAIL;
			goto exit;	
		}
		stats_alloc = 0;
		zbx_json_value_dyn(&jp_path, &stats_name, &stats_alloc);

		if (FAIL == zbx_json_path_open(&jp_row, JSON_PATH_STATISTICS_VALUE, &jp_path)) {
			SET_STR_RESULT(result, strdup(zbx_json_strerror()));
			ret = SYSINFO_RET_FAIL;
			goto exit;	
		}
		stats_alloc = 0;
		zbx_json_value_dyn(&jp_path, &stats_value, &stats_alloc);

		zbx_json_addstring(&j, stats_name, stats_value, ZBX_JSON_TYPE_INT);

		zbx_free(stats_name);
		stats_name = NULL;
		zbx_free(stats_value);
		stats_value = NULL;
	} while(p != NULL);

	zbx_json_close(&j);
	SET_STR_RESULT(result, zbx_strdup(NULL, j.buffer));

exit:
	zbx_json_free(&j);
	free(response.payload);
	return ret;	
}


int zbx_module_ovirt_simple_stats(AGENT_REQUEST *request, AGENT_RESULT *result) {
	char	*service;
	int ret;

	ret = SYSINFO_RET_OK;

	ovirt_log(LOG_LEVEL_DEBUG, "param num [%d]", request->nparam);
	if (1 != request->nparam) {
		SET_MSG_RESULT(result, strdup("Invalid number of parameters."));
		ret = SYSINFO_RET_FAIL;
		goto exit;
	}

	service = get_rparam(request, 0);

	char url[MAX_STRING_LEN];
	strscpy(url, cfg_ovirt_server);
	strscat(url, service);
	strscat(url, "/statistics");

	ret = __get_statistics(url, result);
exit:
	return ret;
}

int zbx_module_ovirt_simple_data(AGENT_REQUEST *request, AGENT_RESULT *result) {
	char	*service;
	int ret;

	ret = SYSINFO_RET_OK;

	ovirt_log(LOG_LEVEL_DEBUG, "param num [%d]", request->nparam);
	if (1 != request->nparam) {
		SET_MSG_RESULT(result, strdup("Invalid number of parameters."));
		ret = SYSINFO_RET_FAIL;
		goto exit;
	}

	service = get_rparam(request, 0);

	char url[MAX_STRING_LEN];
	strscpy(url, cfg_ovirt_server);
	strscat(url, service);

	ret = __get_data(url, result);

exit:
	return ret;
}

int zbx_module_ovirt_host_stats(AGENT_REQUEST *request, AGENT_RESULT *result) {
	char	*service;
	int ret;

	ret = SYSINFO_RET_OK;

	ovirt_log(LOG_LEVEL_DEBUG, "param num [%d]", request->nparam);
	if (1 != request->nparam) {
		SET_MSG_RESULT(result, strdup("Invalid number of parameters."));
		ret = SYSINFO_RET_FAIL;
		goto exit;
	}

	service = get_rparam(request, 0);

	char url[MAX_STRING_LEN];
	strscpy(url, cfg_ovirt_server);
	strscat(url, cfg_ovirt_uri);
	strscat(url, "/hosts/");
	strscat(url, service);
	strscat(url, "/statistics");

	ret = __get_statistics(url, result);
exit:
	return ret;
}

int zbx_module_ovirt_host_data(AGENT_REQUEST *request, AGENT_RESULT *result) {
	char	*service;
	int ret;

	ret = SYSINFO_RET_OK;

	ovirt_log(LOG_LEVEL_DEBUG, "param num [%d]", request->nparam);
	if (1 != request->nparam) {
		SET_MSG_RESULT(result, strdup("Invalid number of parameters."));
		ret = SYSINFO_RET_FAIL;
		goto exit;
	}

	service = get_rparam(request, 0);

	char url[MAX_STRING_LEN];
	strscpy(url, cfg_ovirt_server);
	strscat(url, cfg_ovirt_uri);
	strscat(url, "/hosts/");
	strscat(url, service);

	ret = __get_data(url, result);

exit:
	return ret;
}


int zbx_module_ovirt_vm_stats(AGENT_REQUEST *request, AGENT_RESULT *result) {
	char	*service;
	int ret;

	ret = SYSINFO_RET_OK;

	ovirt_log(LOG_LEVEL_DEBUG, "param num [%d]", request->nparam);
	if (1 != request->nparam) {
		SET_MSG_RESULT(result, strdup("Invalid number of parameters."));
		ret = SYSINFO_RET_FAIL;
		goto exit;
	}

	service = get_rparam(request, 0);

	char url[MAX_STRING_LEN];
	strscpy(url, cfg_ovirt_server);
	strscat(url, cfg_ovirt_uri);
	strscat(url, "/vms/");
	strscat(url, service);
	strscat(url, "/statistics");

	ret = __get_statistics(url, result);
exit:
	return ret;
}

int zbx_module_ovirt_vm_data(AGENT_REQUEST *request, AGENT_RESULT *result) {
	char	*service;
	int ret;

	ret = SYSINFO_RET_OK;

	ovirt_log(LOG_LEVEL_DEBUG, "param num [%d]", request->nparam);
	if (1 != request->nparam) {
		SET_MSG_RESULT(result, strdup("Invalid number of parameters."));
		ret = SYSINFO_RET_FAIL;
		goto exit;
	}

	service = get_rparam(request, 0);

	char url[MAX_STRING_LEN];
	strscpy(url, cfg_ovirt_server);
	strscat(url, cfg_ovirt_uri);
	strscat(url, "/vms/");
	strscat(url, service);

	ret = __get_data(url, result);

exit:
	return ret;
}
