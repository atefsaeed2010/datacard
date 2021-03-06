/*
   Copyright (C) 2011 bg <bg_one@mail.ru>
*/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <sys/types.h>			/* u_int16_t u_int8_t */
#include <dirent.h>			/* DIR */
#include <stdio.h>			/* NULL */
#include <string.h>			/* strlen() */
#include <sys/stat.h>			/* stat() */

#include "pdiscovery.h"			/* pdiscovery_lookup()  */
#include "mutils.h"			/* ITEMS_OF() */
#include "ringbuffer.h"			/* struct ringbuffer */
#include "at_queue.h"			/* write_all() */
#include "at_read.h"			/* at_wait() at_read() at_read_result_iov() at_read_result_classification() */
#include "chan_datacard.h"		/* opentty() closetty() */

/* timeout for port readering milliseconds */
#define PDISCOVERY_TIMEOUT		500

enum INTERFACE_TYPE {
	INTERFACE_TYPE_DATA	= 0,
	INTERFACE_TYPE_VOICE,
	INTERFACE_TYPE_COM,
	INTERFACE_TYPE_NUMBERS,
};

struct pdiscovery_device {
	u_int16_t	vendor_id;
	u_int16_t	product_id;
	u_int8_t	interfaces[INTERFACE_TYPE_NUMBERS];
};

struct pdiscovery_ports {
	char		* ports[INTERFACE_TYPE_NUMBERS];
};

struct pdiscovery_info {
	char		* imei;
	char		* imsi;
};

struct pdiscovery_req {
	const char	* name;
	const char	* imei;
	const char	* imsi;
	char		** dport;
	char 		** aport;
};


struct cache_item {
	AST_LIST_ENTRY (cache_item)    entry;
	char	* port;
	char	* imei;
	char	* imsi;
	int	status;
	struct timeval validtill;
};

struct discovery_cache {
	AST_LIST_HEAD_NOLOCK (, cache_item)  items;
};



#define BUILD_NAME(d1, d2, d1len, d2len, out)		\
		d2len = strlen(d2);			\
		out = alloca(d1len + 1 + d2len + 1);	\
		memcpy(out, d1, d1len);			\
		out[d1len] = '/';			\
		memcpy(out + d1len + 1, d2, d2len);	\
		d2len += d1len + 1;			\
		out[d2len] = 0;


static const struct pdiscovery_device device_ids[] = {
	{ 0x12d1, 0x1001, { 2, 1, 0 } },
	{ 0x12d1, 0x140c, { 3, 2, 0 } },
};

static struct discovery_cache cache;


#/* */
static void cache_item_destroy(struct cache_item * item)
{
	if(item) {
		if(item->imsi)
			ast_free(item->imsi);
		if(item->imei)
			ast_free(item->imei);
		if(item->port)
			ast_free(item->port);
		ast_free(item);
	}
}

#/* */
static void cache_item_update(struct cache_item * item, const char * imei, const char * imsi, int status)
{
	if(item->imsi)
		ast_free(item->imsi);
	if(item->imei)
		ast_free(item->imei);

	if(imei)
		item->imei = ast_strdup(imei);
	if(imsi)
		item->imsi = ast_strdup(imsi);

	item->status = status;

	item->validtill = ast_tvnow();
	item->validtill.tv_sec += CONF_GLOBAL(discovery_interval);
}

#/* */
static struct cache_item * cache_item_create(const char * port, const char * imei, const char * imsi, int status)
{
	struct cache_item * item = ast_calloc(1, sizeof(*item));
	if(item) {
		item->port = ast_strdup(port);
		if(item->port) {
			cache_item_update(item, imei, imsi, status);
		} else {
			cache_item_destroy(item);
			item = 0;
		}
	}
	
	return item;
}


#/* */
static struct cache_item * cache_search(struct discovery_cache * cache, const char * port)
{
	struct cache_item * item;
	
	struct timeval now = ast_tvnow();
	
	AST_LIST_TRAVERSE_SAFE_BEGIN(&cache->items, item, entry) {
		if(strcmp(item->port, port) == 0) {
			if(ast_tvcmp(now, item->validtill) < 0) {
				return item;
			} else {
				// remove item
				AST_LIST_REMOVE_CURRENT(entry);
				cache_item_destroy(item);
				break;
			}
		}
	}
	AST_LIST_TRAVERSE_SAFE_END;
	
	return NULL;
}


#/* */
static int cache_lookup(struct discovery_cache * cache, const char * port, struct pdiscovery_req * req, struct pdiscovery_info * info, int * status)
{
	int found = 0;
	struct cache_item * item = cache_search(cache, port);
	if(item) {
		info->imei = item->imei ? ast_strdup(item->imei) : 0;
		info->imsi = item->imsi ? ast_strdup(item->imsi) : 0;
		found = (!req->imei || !req->imei[0] || item->imei) && (!req->imsi || !req->imsi[0] || item->imsi);
		if(found) {
			*status = item->status;
		}
	}
	return found;
}


static void cache_update(struct discovery_cache * cache, const char * port, const struct pdiscovery_info * info, int status)
{
	struct cache_item * item = cache_search(cache, port);
	if(item) {
		cache_item_update(item, info->imei, info->imsi, status);
	} else {
		item = cache_item_create(port, info->imei, info->imsi, status);
		AST_LIST_INSERT_TAIL(&cache->items, item, entry);
	}
}


#/* */
static void cache_init(struct discovery_cache * cache)
{
	/* TODO: place lock init when locking becomes required */
	
	AST_LIST_HEAD_INIT_NOLOCK(&cache->items);
}

#/* */
static void cache_fini(struct discovery_cache * cache)
{
	struct cache_item * item;
	AST_LIST_TRAVERSE_SAFE_BEGIN(&cache->items, item, entry) {
		AST_LIST_REMOVE_CURRENT(entry);
		cache_item_destroy(item);
	}
	AST_LIST_TRAVERSE_SAFE_END;	
}

#/* */
static int pdiscovery_get_id(const char * name, int len, const char * filename, unsigned * integer)
{
	int len2;
	char * name2;
	int assign = 0;

	BUILD_NAME(name, filename, len, len2, name2);
	FILE * file = fopen(name2, "r");
	if(file) {
		assign = fscanf(file, "%x", integer);
		fclose(file);
	}

	return assign;
}

#/* */
static const const struct pdiscovery_device * pdiscovery_lookup_ids(const char * devname, const char * name, int len)
{
	unsigned vid;
	unsigned pid;
	unsigned idx;

	if(pdiscovery_get_id(name, len, "idVendor", &vid) == 1 && pdiscovery_get_id(name, len, "idProduct", &pid) == 1) {
		ast_debug(4, "[%s discovery] found %s is idVendor %04x idProduct %04x\n", devname, name, vid, pid);
		for(idx = 0; idx < ITEMS_OF(device_ids); idx++) {
			if(device_ids[idx].vendor_id == vid && device_ids[idx].product_id == pid) {
				return &device_ids[idx];
				}
		}
	}
	return NULL;
}

#/* */
static int pdiscovery_is_port(const char * name, int len)
{
	int len2;
	char * name2;
	struct stat statb;

	BUILD_NAME(name, "port_number", len, len2, name2);
	return stat(name2, &statb) == 0 && S_ISREG(statb.st_mode);
}

#/* */
static char * pdiscovery_port(const char * name, int len, const char * subdir)
{
	int len2, len3;
	char * name2, * name3;
	struct stat statb;
	char * port = NULL;

	BUILD_NAME(name, subdir, len, len2, name2);

	if(stat(name2, &statb) == 0 && S_ISDIR(statb.st_mode) && pdiscovery_is_port(name2, len2)) {
//		ast_debug(4, "[%s discovery] found port %s\n", devname, dentry->d_name);
		BUILD_NAME("/dev", subdir, 4, len3, name3);
		port = ast_strdup(name3);
	}
	return port;
}

#/* */
static char * pdiscovery_port_name(const char * name, int len)
{
	char * port = NULL;
	struct dirent * dentry;
	DIR * dir = opendir(name);
	if(dir) {
		while((dentry = readdir(dir)) != NULL) {
			if(strcmp(dentry->d_name, ".") != 0 && strcmp(dentry->d_name, "..") != 0) {
				port = pdiscovery_port(name, len, dentry->d_name);
				if(port)
					break;
			}
		}
		closedir(dir);
	}
	return port;
}

#/* */
static char * pdiscovery_interface(const char * name, int len, unsigned * interface)
{
	char * port = NULL;
	if(pdiscovery_get_id(name, len, "bInterfaceNumber", interface) == 1) {
//		ast_debug(4, "[%s discovery] bInterfaceNumber %02x\n", devname, *interface);
		port = pdiscovery_port_name(name, len);
	}
	return port;
}

#/* */
static char * pdiscovery_find_port(const char * name, int len, const char * subdir, unsigned * interface)
{
	int len2;
	char * name2;
	struct stat statb;
	char * port = NULL;

	BUILD_NAME(name, subdir, len, len2, name2);

	if(stat(name2, &statb) == 0 && S_ISDIR(statb.st_mode)) {
		port = pdiscovery_interface(name2, len2, interface);
	}
	return port;
}

#/* */
static int pdiscovery_interfaces(const char * devname, const char * name, int len, const struct pdiscovery_device * device, struct pdiscovery_ports * ports)
{
	unsigned interface;
	unsigned idx;
	int found = 0;
	struct dirent * dentry;
	char * port;

	DIR * dir = opendir(name);
	if(dir) {
		while((dentry = readdir(dir)) != NULL) {
			if(strchr(dentry->d_name, ':')) {
				port = pdiscovery_find_port(name, len, dentry->d_name, &interface);
				if(port) {
					ast_debug(4, "[%s discovery] found InterfaceNumber %02x port %s\n", devname, interface, port);
					for(idx = 0; idx < (int)ITEMS_OF(device->interfaces); idx++) {
						if(device->interfaces[idx] == interface) {
							if(ports->ports[idx] == NULL) {
								ports->ports[idx] = port;
								if(++found == INTERFACE_TYPE_NUMBERS)
									break;
							} else {
								ast_debug(4, "[%s discovery] port %s for bInterfaceNumber %02x already exists new is %s\n", devname, ports->ports[idx], interface, port);
								// FIXME
							}
						}
					}
				}
			}
		}
		closedir(dir);
	}
	return found;
}

#/* */
static char * pdiscovery_handle_ati(const char * devname, char * str, char ** imei2)
{
	char * imei = strstr(str, "IMEI:");

	if(imei) {
		imei += 5;
		while(imei[0] == ' ')
			imei++;
		str = imei;

		while(str[0] >= '0' && str[0] <= '9')
			str++;
		if(str - imei == IMEI_SIZE) {
			char save = str[0];
			str[0] = 0;
			*imei2 = ast_strdup(imei);
			ast_debug(4, "[%s discovery] found IMEI %s\n", devname, imei);
			str[0] = save;
		}

	}
	return str;
}

#/* */
static void pdiscovery_handle_cimi(const char * devname, char * str, char ** imsi2)
{
	char * imsi;

	while(str[0] < '0' || str[0] > '9') {
		if(str[0] == 0)
			return;
		str++;
	}
	imsi = str;

	while(str[0] >= '0' && str[0] <= '9')
		str++;
	if(str - imsi == IMSI_SIZE && str[0] == '\r' && imsi[-1] == '\n') {
		str[0] = 0;
		*imsi2 = ast_strdup(imsi);
		ast_debug(4, "[%s discovery] found IMSI %s\n", devname, imsi);
	}
}

#/* return non-zero on done with command */
static int pdiscovery_handle_response(struct pdiscovery_req * req, const struct iovec iov[2], int iovcnt, struct pdiscovery_info * info)
{
	int done = 0;
	char * str;
	size_t len = iov[0].iov_len + iov[1].iov_len;
	if(len > 0) {
		len--;
		if(iovcnt == 2) {
			str = alloca(len + 1);
			if(!str) {
				return 1;
			memcpy(str, iov[0].iov_base, iov[0].iov_len);
			memcpy(str + iov[0].iov_len, iov[1].iov_base, iov[1].iov_len);
			}
		} else {
			str = iov[0].iov_base;
		}
		str[len] = 0;

		ast_debug(4, "[%s discovery] < %s\n", req->name, str);
		done = strstr(str, "OK") != NULL || strstr(str, "ERROR") != NULL;
		if(req->imei && req->imei[0] && !info->imei)
			str = pdiscovery_handle_ati(req->name, str, &info->imei);
		if(req->imsi && req->imsi[0] && !info->imsi)
			pdiscovery_handle_cimi(req->name, str, &info->imsi);
	}
	return done;
}

#/* return zero on sucess */
static int pdiscovery_do_cmd(struct pdiscovery_req * req, int fd, const char * name, const char * cmd, unsigned length, struct pdiscovery_info * info)
{
	int timeout;
	char buf[2*1024];
	struct ringbuffer rb;
	struct iovec iov[2];
	int iovcnt;
	size_t wrote;

	ast_debug(4, "[%s discovery] use %s for IMEI/IMSI discovery\n", req->name, name);

	clean_read_data(req->name, fd);
	wrote = write_all(fd, cmd, length);
	if(wrote == length) {
		timeout = PDISCOVERY_TIMEOUT;
		rb_init(&rb, buf, sizeof (buf));
		while(timeout > 0 && at_wait(fd, &timeout) != 0) {
			iovcnt = at_read(fd, name, &rb);
			if(iovcnt > 0) {
				iovcnt = rb_read_all_iov(&rb, iov);
				if(pdiscovery_handle_response(req, iov, iovcnt, info))
					return 0;
			} else {
				ast_log (LOG_ERROR, "[%s discovery] read from %s failed: %s\n", req->name, name, strerror(errno));
				return -1;
			}
		}
		ast_log (LOG_ERROR, "[%s discovery] failed to get valid response from %s in %d msec\n", req->name, name, PDISCOVERY_TIMEOUT);
	} else {
		ast_log (LOG_ERROR, "[%s discovery] write to %s failed: %s\n", req->name, name, strerror(errno));
	}
	return -1;
}


#/* return non-zero on fail */
static int pdiscovery_get_info(const char * port, struct pdiscovery_req * req, struct pdiscovery_info * info)
{
	static const struct {
		const char	* cmd;
		unsigned	length;
	} cmds[] = {
		{ "AT+CIMI\r", 8 },		/* IMSI */
		{ "ATI\r", 4 },			/* IMEI */
		{ "ATI; +CIMI\r" , 11 },	/* IMSI + IMEI */
	};

	static const int want_map[2][2] = {
		{ 2, 0 },	// want_imei = 0
		{ 1, 2 }	// want_imei = 1
	};

	int fail = -1;
	char * lock_file;

	int fd = opentty(port, &lock_file);
	if(fd >= 0) {
		unsigned want_imei = req->imei && req->imei[0] && ! info->imei;
		unsigned want_imsi = req->imsi && req->imsi[0] && ! info->imsi;
		unsigned cmd = want_map[want_imei][want_imsi];
		
		/* clean queue first ? */
		fail = pdiscovery_do_cmd(req, fd, port, cmds[cmd].cmd, cmds[cmd].length, info);
		closetty(fd, &lock_file);
	}
	
	return fail;
}

#/* return non-zero on fail */
static int pdiscovery_get_info_cached(const char * port, struct pdiscovery_req * req, struct pdiscovery_info * info)
{
	int fail;
	int found = cache_lookup(&cache, port, req, info, &fail);
	if(!found) {
		fail = pdiscovery_get_info(port, req, info);
		cache_update(&cache, port, info, fail);
	} else {
		ast_debug(4, "[%s discovery] %s use cached IMEI %s IMSI %s\n", req->name, port, S_OR(info->imei, ""), S_OR(info->imsi, ""));
	}
		
	return fail;
}

#/* return zero on success */
static int pdiscovery_read_info(struct pdiscovery_req * req, struct pdiscovery_ports * ports, struct pdiscovery_info * info)
{

	char * dlock;
	int fail = -1;
	const char * cport = ports->ports[INTERFACE_TYPE_COM];
	const char * dport = ports->ports[INTERFACE_TYPE_DATA];


	if(cport && strcmp(cport, dport) != 0) {
		int pid = lock_try(dport, &dlock);
		if(pid == 0) {
			fail = pdiscovery_get_info_cached(cport, req, info);
			closetty(-1, &dlock);
		} else {
			ast_debug(4, "[%s discovery] %s already used by process %d, skipped\n", req->name, dport, pid);
//			ast_log (LOG_WARNING, "[%s discovery] %s already used by process %d\n", devname, dport, pid);
		}
	} else {
		fail = pdiscovery_get_info_cached(dport, req, info);
	}
	return fail;
}

#ifndef STANDALONE_GLOBAL_SEARCH
#/* */
static int pdiscovery_check_req(struct pdiscovery_ports * ports, struct pdiscovery_req * req)
{
	struct pdiscovery_info info = { 0, 0 };

	int match = 0;
	if(pdiscovery_read_info(req, ports, &info) == 0) {

		match = ((req->imei == NULL || req->imei[0] == 0) || (info.imei && strcmp(req->imei, info.imei) == 0))
			&&
			    ((req->imsi == NULL || req->imsi[0] == 0) || (info.imsi && strcmp(req->imsi, info.imsi) == 0));
		if(match) {
			*req->dport = ports->ports[INTERFACE_TYPE_DATA];
			*req->aport = ports->ports[INTERFACE_TYPE_VOICE];
		}
		ast_debug(4, "[%s discovery] %smatched rIMEI=%s fIMEI=%s rIMSI=%s fIMSI=%s\n",
			req->name,
			match ? "" : "un" , 
			req->imei , S_OR(info.imei, ""),
			req->imsi,  S_OR(info.imsi, "")
			);
	}

	if(info.imei)
		ast_free(info.imei);
	if(info.imsi)
		ast_free(info.imsi);
	
	return match;
}
#endif /* STANDALONE_GLOBAL_SEARCH */

#/* */
static int pdiscovery_check_device(const char * name, int len, const char * subdir, struct pdiscovery_req * req)
{
	int len2;
	char * name2;
	const struct pdiscovery_device * device;
	int found = 0;

	BUILD_NAME(name, subdir, len, len2, name2);

	device = pdiscovery_lookup_ids(req->name, name2, len2);
	if(device) {
		struct pdiscovery_ports ports;
		memset(&ports, 0, sizeof(ports));

		ast_debug(4, "[%s discovery] should interfaces -> ports map for %04x:%04x modemI=%02x voiceI=%02x dataI=%02x\n", 
			req->name,
			device->vendor_id, 
			device->product_id, 
			device->interfaces[INTERFACE_TYPE_COM],
			device->interfaces[INTERFACE_TYPE_VOICE],
			device->interfaces[INTERFACE_TYPE_DATA]
			);
		pdiscovery_interfaces(req->name, name2, len2, device, &ports);

		/* check mandatory ports */
		if(ports.ports[INTERFACE_TYPE_DATA] && ports.ports[INTERFACE_TYPE_VOICE]) {
			found = pdiscovery_check_req(&ports, req);
			if(!found) {
				for(len2 = 0; len2 < (int)ITEMS_OF(ports.ports); len2++) {
					if(ports.ports[len2]) {
						ast_free(ports.ports[len2]);
					}
				}
			}
		}
	}
	return found;
}

#/* */
static int pdiscovery_req(const char * name, int len, struct pdiscovery_req * req)
{
	int found = 0;
	struct dirent * dentry;
	DIR * dir = opendir(name);
	if(dir) {
		while((dentry = readdir(dir)) != NULL) {
			if(strcmp(dentry->d_name, ".") != 0 && strcmp(dentry->d_name, "..") != 0 && strstr(dentry->d_name, "usb") != dentry->d_name) {
				ast_debug(4, "[%s discovery] checking %s/%s\n", req->name, name, dentry->d_name);
				found = pdiscovery_check_device(name, len, dentry->d_name, req);
				if(found)
					break;
			}
		}
		closedir(dir);
	}
	return found;
}



#/* */
EXPORT_DEF int pdiscovery_lookup(const char * devname, const char * imei, const char * imsi, char ** dport, char ** aport)
{
/*	static const char sys_bus_usb_drivers_usb[] = "/sys/bus/usb/drivers/usb"; */
	static const char sys_bus_usb_devices[] = "/sys/bus/usb/devices";

	struct pdiscovery_req req = {
		devname, 
		imei, 
		imsi, 
		dport, 
		aport 
		};

	return pdiscovery_req(sys_bus_usb_devices, strlen(sys_bus_usb_devices), &req);
}

#/* */
EXPORT_DEF void pdiscovery_init()
{
	cache_init(&cache);
}

#/* */
EXPORT_DEF void pdiscovery_fini()
{
	cache_fini(&cache);
}
