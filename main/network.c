/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain
 * this notice you can do whatever you want with this stuff. If we meet some day,
 * and you think this stuff is worth it, you can buy me a beer in return.
 * 
 * Adapted by William Peters in 2021
 * ----------------------------------------------------------------------------
 */

#include <libesphttpd/esp.h>
#include "libesphttpd/httpd.h"
//#include "io.h"

#ifdef CONFIG_ESPHTTPD_USE_ESPFS
#include "espfs.h"
#include "espfs_image.h"
#include "libesphttpd/httpd-espfs.h"
#endif // CONFIG_ESPHTTPD_USE_ESPFS

#include "libesphttpd/cgiwifi.h"
#include "libesphttpd/cgiflash.h"
#include "libesphttpd/auth.h"
#include "libesphttpd/captdns.h"
#include "libesphttpd/cgiwebsocket.h"
#include "libesphttpd/httpd-freertos.h"
#include "libesphttpd/route.h"

#include "esp_wifi.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#ifdef ESP32
#include "freertos/event_groups.h"
#include "esp_log.h"

#define CONFIG_STORE_WIFI_TO_NVS
#ifdef CONFIG_STORE_WIFI_TO_NVS
#include "nvs_flash.h"
#define NVS_NAMESPACE "nvs"
nvs_handle my_nvs_handle;
#define NET_CONF_KEY "netconf"
#endif

#include "esp_err.h"
#include "esp_event.h"
#include "esp_netif.h"

// application includes
#include "include/mbus.h"
#include "include/mbusEmu.h"
#include "include/io.h"
#include "include/network.h"

bool netDebug=true;

char my_hostname[16] = "esphttpd";

/* The examples use simple WiFi configuration that you can set via
   'make menuconfig'.
   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define DEFAULT_WIFI_STA_SSID "mywifissid"
*/
#define DEFAULT_WIFI_MODE WIFI_MODE_AP
#define DEFAULT_WIFI_STA_SSID "test"
#define DEFAULT_WIFI_STA_PASS "password"
#define EXAMPLE_ESP_MAXIMUM_RETRY  0
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

/* FreeRTOS event group to signal when we are connected*/
#endif



#define TAG "user_main"

#define LISTEN_PORT     80u
#define MAX_CONNECTIONS 32u

static char connectionMemory[sizeof(RtosConnType) * MAX_CONNECTIONS];
static HttpdFreertosInstance httpdFreertosInstance;


void wsLog(char *logMsg) {
	cgiWebsockBroadcast(&httpdFreertosInstance.httpdInstance, "/ws.cgi", logMsg, strlen(logMsg), WEBSOCK_FLAG_NONE);
}

void wsSendData(mbus_packet_t *buffer) {

	char buff[128];
	sprintf(buff, "MBUSRX: ");
	for (int i=0; i<buffer->len+1; i++) {
		sprintf(buff + strlen(buff), "%c", buffer->message[i]);
	}
	sprintf(buff + strlen(buff), "\n");

	cgiWebsockBroadcast(&httpdFreertosInstance.httpdInstance, "/ws.cgi", buff, strlen(buff), WEBSOCK_FLAG_NONE);
}

void handleWebSocketCmd(char *cmd) {
	if(netDebug) printf("WebSocket: strlen:%i cmd:%s\n", strlen(cmd), cmd);
	if(strcmp (cmd,"enableDebug")==0) {
		network_debug(true);
		mbus_debug(true);
		mbusEmu_debug(true);
		io_debug(true);
		printf("wsCmd: enableDebug\n");

	} else if(strcmp (cmd,"disableDebug")==0) {
		network_debug(false);
		mbus_debug(false);
		mbusEmu_debug(false);
		io_debug(false);
		printf("wsCmd: disableDebug\n");

	} else if(strcmp (cmd,"emuDisable")==0) {
		printf("wsCmd: passthroughEnabled\n");
		setPassthroughMode(true);

	} else if(strcmp (cmd,"emuEnable")==0) {
		printf("wsCmd: passthroughDisabled\n");
		setPassthroughMode(false);

	} else if(strcmp (cmd,"emuPcktEnable")==0) {
		cdcEmuPcktEnable();

	} else if(strcmp (cmd,"emuPcktDisable")==0) {
		cdcEmuPcktDisable();

	} else if(strcmp (cmd,"cdcNext")==0) {
		cdcChangeTrackUp();

	} else if(strcmp (cmd,"cdcPrev")==0) {
		cdcChangeTrackDown();

	} else if(strcmp (cmd,"cdcPwrDn")==0) {
		cdcChangePwrDn();

	} else if(strcmp (cmd,"cdcPlay")==0) {
		cdcChangePlay();

	} else if(strcmp (cmd,"cdcMagEject")==0) {
		cdcChangeEjectMag();

	} else if(strcmp (cmd,"cdcMagInsert")==0) {
		cdcChangeInsertMag();

	} else if(strcmp (cmd,"rLoadDisk1")==0) {
		rLoadDisk(1);

	} else if(strcmp (cmd,"rLoadDisk2")==0) {
		rLoadDisk(2);

	} else if(strcmp (cmd,"rLoadDisk3")==0) {
		rLoadDisk(3);

	} else if(strcmp (cmd,"rLoadDisk4")==0) {
		rLoadDisk(4);

	} else if(strcmp (cmd,"rLoadDisk5")==0) {
		rLoadDisk(5);

	} else if(strcmp (cmd,"rLoadDisk6")==0) {
		rLoadDisk(6);

	} else if(strcmp (cmd,"cdcLoadDisk1")==0) {
		cdcChangeLoadDisk(1);

	} else if(strcmp (cmd,"cdcLoadDisk2")==0) {
		cdcChangeLoadDisk(2);

	} else if(strcmp (cmd,"cdcLoadDisk3")==0) {
		cdcChangeLoadDisk(3);

	} else if(strcmp (cmd,"cdcLoadDisk4")==0) {
		cdcChangeLoadDisk(4);

	} else if(strcmp (cmd,"cdcLoadDisk5")==0) {
		cdcChangeLoadDisk(5);

	} else if(strcmp (cmd,"cdcLoadDisk6")==0) {
		cdcChangeLoadDisk(6);

	} else if(strcmp (cmd,"phoneModeEnable")==0) {
		emuPhoneMode(true);

	} else if(strcmp (cmd,"phoneModeDisable")==0) {
		emuPhoneMode(false);
		
	} else if(strcmp (cmd,"intcdModeEnable")==0) {
		emuIntCDMode(true);

	} else if(strcmp (cmd,"btModeEnable")==0) {
		emuBTMode(true);

	} else {
		printf("wsCmd: unknownCmd: %s\n", cmd);
	}
}

//On reception of a message, send "You sent: " plus whatever the other side sent
static void myWebsocketRecv(Websock *ws, char *data, int len, int flags) {
	int i;
	char buff[128];
	char cmd[128];
	sprintf(buff, "You sent: ");
	for (i=0; i<len; i++){
		buff[i+10]=data[i];
		cmd[i]=data[i];
	}
	cmd[i]=0; // terminate string.
	buff[i+10]=0;
	handleWebSocketCmd(cmd);
	cgiWebsocketSend(&httpdFreertosInstance.httpdInstance,
	                 ws, buff, strlen(buff), WEBSOCK_FLAG_NONE);
}

//Websocket connected. Install reception handler and send welcome message.
static void myWebsocketConnect(Websock *ws) {
	ws->recvCb=myWebsocketRecv;
	cgiWebsocketSend(&httpdFreertosInstance.httpdInstance,
	                 ws, "Hi, Websocket!", 14, WEBSOCK_FLAG_NONE);
}

static void customHeaders_cacheForever(HttpdConnData *connData)
{
	httpdHeader(connData, "Cache-Control", "max-age=365000000, public, immutable");
	httpdHeader(connData, "Content-Type", httpdGetMimetype(connData->url));
	ESP_LOGV(TAG, "customHeaders_cacheForever");
}

HttpdCgiExArg CgiOptionsEspfsStatic = {
    .headerCb = &customHeaders_cacheForever,
	.basepath = "/static/"
};


//CGI flash
#define OTA_FLASH_SIZE_K 1024
#define OTA_TAGNAME "generic"

int flashPass(HttpdConnData *connData, int no, char *user, int userLen, char *pass, int passLen) {
	if (no==0) {
		strcpy(user, "admin");
		strcpy(pass, "admin");
		return 1;
	}
	return 0;
}

CgiUploadFlashDef uploadParams={
	.type=CGIFLASH_TYPE_FW,
	.fw1Pos=0x1000,
	.fw2Pos=((OTA_FLASH_SIZE_K*1024)/2)+0x1000,
	.fwSize=((OTA_FLASH_SIZE_K*1024)/2)-0x1000,
	.tagName=OTA_TAGNAME
};


//Template code for the counter on the index page.
static int hitCounter=0;
CgiStatus ICACHE_FLASH_ATTR tplCounter(HttpdConnData *connData, char *token, void **arg) {
	char buff[128];
	if (token==NULL) return HTTPD_CGI_DONE;

	if (strcmp(token, "counter")==0) {
		hitCounter++;
		sprintf(buff, "%d", hitCounter);
	}
	httpdSend(connData, buff, -1);
	return HTTPD_CGI_DONE;
}
//

CgiStatus tplCounter(HttpdConnData *connData, char *token, void **arg);
HttpdBuiltInUrl builtInUrls[] = {
	ROUTE_CGI_ARG("*", cgiRedirectApClientToHostname, "esp8266.nonet"),
	ROUTE_REDIRECT("/", "/index.tpl"),

	ROUTE_TPL("/index.tpl", tplCounter),

	//Routines to make the /wifi URL and everything beneath it work.
	//Enable the line below to protect the WiFi configuration with an username/password combo.
	//	{"/wifi/*", authBasic, myPassFn},

	//ROUTE_REDIRECT("/websocket", "/websocket/index.html"),
	ROUTE_WS("/ws.cgi", myWebsocketConnect),
	//ROUTE_WS("/websocket/echo.cgi", myEchoWebsocketConnect),

	// Files in /static dir are assumed to never change, so send headers to encourage browser to cache forever.
	ROUTE_FILE_EX("/static/*", &CgiOptionsEspfsStatic),


	//{"/flash/*", authBasic, flashPass},
	ROUTE_REDIRECT("/flash", "/flash/index.html"),
	ROUTE_REDIRECT("/flash/", "/flash/index.html"),
	ROUTE_CGI("/flash/flashinfo.json", cgiGetFlashInfo),
	ROUTE_CGI("/flash/setboot", cgiSetBoot),
	ROUTE_CGI_ARG("/flash/upload", cgiUploadFirmware, &uploadParams),
	ROUTE_CGI_ARG("/flash/erase", cgiEraseFlash, &uploadParams),
	ROUTE_CGI("/flash/reboot", cgiRebootFirmware),

	ROUTE_FILESYSTEM(),

	ROUTE_END()
};

#define EXAMPLE_ESP_WIFI_SSID      DEFAULT_WIFI_STA_SSID
#define EXAMPLE_ESP_WIFI_PASS      DEFAULT_WIFI_STA_PASS
#define EXAMPLE_ESP_WIFI_CHANNEL   6
#define EXAMPLE_MAX_STA_CONN       4

static const char *TAG2 = "wifi softAP";

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG2, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG2, "station "MACSTR" leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

void wifi_init_softap(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .channel = EXAMPLE_ESP_WIFI_CHANNEL,
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG2, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS, EXAMPLE_ESP_WIFI_CHANNEL);
}



void network_setup(void) {
	printf("[NETWORK] Initializing...\t\t");
	esp_err_t err;
	// Init NVS
	err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
	{
		// NVS partition was truncated and needs to be erased
		// Retry nvs_flash_init
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
	}
	ESP_ERROR_CHECK(err);

	uint32_t net_configured = 0;	 // value will default to 0, if not set yet in NVS
	ESP_LOGI(TAG, "Opening NVS handle ");
	err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &my_nvs_handle);
	if (err != ESP_OK)
	{
		ESP_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
	}
	else
	{

		// Read NVS
		ESP_LOGI(TAG, "Reading network initialization from NVS");
		err = nvs_get_u32(my_nvs_handle, NET_CONF_KEY, &net_configured);
		switch (err)
		{
		case ESP_OK:
			ESP_LOGI(TAG, "nvs init = %d", net_configured);
			break;
		case ESP_ERR_NVS_NOT_FOUND:
			ESP_LOGI(TAG, "nvs init not found, initializing now.");
			break;
		default:
			ESP_LOGE(TAG, "Error (%s) reading!\n", esp_err_to_name(err));
		}
	}

//#ifdef CONFIG_ESPHTTPD_USE_ESPFS
	EspFsConfig espfs_conf = {
		.memAddr = espfs_image_bin,
	};
	EspFs* fs = espFsInit(&espfs_conf);
    httpdRegisterEspfs(fs);
//#endif // CONFIG_ESPHTTPD_USE_ESPFS

	esp_netif_init();

	// init and start webserver
	httpdFreertosInit(&httpdFreertosInstance,
	                  builtInUrls,
	                  LISTEN_PORT,
	                  connectionMemory,
	                  MAX_CONNECTIONS,
	                  HTTPD_FLAG_NONE);

	httpdFreertosStart(&httpdFreertosInstance);


	ESP_ERROR_CHECK(initCgiWifi()); // Initialise wifi configuration CGI

	//net_configured=0;
	wifi_init_softap();
	captdnsInit();

	// if (!net_configured)
	// { // If wasn't initialized, now we are initialized.  Write it to NVS.
	// 	net_configured = 1;
	// 	ESP_LOGI(TAG, "Writing init to NVS");
	// 	ESP_ERROR_CHECK(nvs_set_u32(my_nvs_handle, NET_CONF_KEY, net_configured));
	// 	// After setting any values, nvs_commit() must be called to ensure changes are written to flash storage.
	// 	ESP_LOGI(TAG, "Committing updates in NVS");
	// 	ESP_ERROR_CHECK(nvs_commit(my_nvs_handle));
	// 	// Close NVS
	// 	//nvs_close(my_nvs_handle); - don't close if handle is shared by cgi_NVS
	// }
	printf("[ DONE ]\n");
}


void network_debug(bool newState) {
  netDebug=true;
}
