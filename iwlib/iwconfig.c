/*
 * Copyright (C) 2009 Red Hat Inc.
 *
 * Interface with iwlib from rhpl by Harald Hoyer <harald@redhat.com>.
 *
 * This application is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; version 2.
 *
 * This application is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <Python.h>

#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <iwlib.h>
#include <linux/sockios.h>

/*------------------------------------------------------------------*/
/*
 * Get wireless informations & config from the device driver
 * We will call all the classical wireless ioctl on the driver through
 * the socket to know what is supported and to get the settings...
 */
static int
get_info(int			skfd,
	 char *			ifname,
	 struct wireless_info *	info)
{
  struct iwreq		wrq;

  memset((char *) info, 0, sizeof(struct wireless_info));

  /* Get wireless name */
  if(iw_get_ext(skfd, ifname, SIOCGIWNAME, &wrq) < 0)
    {
      /* If no wireless name : no wireless extensions */
      /* But let's check if the interface exists at all */
      struct ifreq ifr;

      strncpy(&ifr.ifr_name[0], ifname, IFNAMSIZ);
      ifr.ifr_name[IFNAMSIZ-1] = 0;
      if(ioctl(skfd, SIOCGIFFLAGS, &ifr) < 0)
	return(-ENODEV);
      else
	return(-ENOTSUP);
    }
  else {
    strncpy(info->b.name, wrq.u.name, sizeof(info->b.name));
    info->b.name[sizeof(info->b.name)-1] = 0;
  }

  /* Get ranges */
  if(iw_get_range_info(skfd, ifname, &(info->range)) >= 0)
    info->has_range = 1;

  /* Get network ID */
  if(iw_get_ext(skfd, ifname, SIOCGIWNWID, &wrq) >= 0)
    {
      info->b.has_nwid = 1;
      memcpy(&(info->b.nwid), &(wrq.u.nwid), sizeof(iwparam));
    }

  /* Get frequency / channel */
  if(iw_get_ext(skfd, ifname, SIOCGIWFREQ, &wrq) >= 0)
    {
      info->b.has_freq = 1;
      info->b.freq = iw_freq2float(&(wrq.u.freq));
    }

  /* Get sensitivity */
  if(iw_get_ext(skfd, ifname, SIOCGIWSENS, &wrq) >= 0)
    {
      info->has_sens = 1;
      memcpy(&(info->sens), &(wrq.u.sens), sizeof(iwparam));
    }

  /* Get encryption information */
  wrq.u.data.pointer = (caddr_t) info->b.key;
  wrq.u.data.length = IW_ENCODING_TOKEN_MAX;
  wrq.u.data.flags = 0;
  if(iw_get_ext(skfd, ifname, SIOCGIWENCODE, &wrq) >= 0)
    {
      info->b.has_key = 1;
      info->b.key_size = wrq.u.data.length;
      info->b.key_flags = wrq.u.data.flags;
    }

  /* Get ESSID */
  wrq.u.essid.pointer = (caddr_t) info->b.essid;
  wrq.u.essid.length = IW_ESSID_MAX_SIZE;
  wrq.u.essid.flags = 0;
  if(iw_get_ext(skfd, ifname, SIOCGIWESSID, &wrq) >= 0)
    {
      info->b.has_essid = 1;
      info->b.essid_on = wrq.u.data.flags;
    }

  /* Get AP address */
  if(iw_get_ext(skfd, ifname, SIOCGIWAP, &wrq) >= 0)
    {
      info->has_ap_addr = 1;
      memcpy(&(info->ap_addr), &(wrq.u.ap_addr), sizeof (sockaddr));
    }

  /* Get NickName */
  wrq.u.essid.pointer = (caddr_t) info->nickname;
  wrq.u.essid.length = IW_ESSID_MAX_SIZE;
  wrq.u.essid.flags = 0;
  if(iw_get_ext(skfd, ifname, SIOCGIWNICKN, &wrq) >= 0)
    if(wrq.u.data.length > 1)
      info->has_nickname = 1;

  /* Get bit rate */
  if(iw_get_ext(skfd, ifname, SIOCGIWRATE, &wrq) >= 0)
    {
      info->has_bitrate = 1;
      memcpy(&(info->bitrate), &(wrq.u.bitrate), sizeof(iwparam));
    }

  /* Get RTS threshold */
  if(iw_get_ext(skfd, ifname, SIOCGIWRTS, &wrq) >= 0)
    {
      info->has_rts = 1;
      memcpy(&(info->rts), &(wrq.u.rts), sizeof(iwparam));
    }

  /* Get fragmentation threshold */
  if(iw_get_ext(skfd, ifname, SIOCGIWFRAG, &wrq) >= 0)
    {
      info->has_frag = 1;
      memcpy(&(info->frag), &(wrq.u.frag), sizeof(iwparam));
    }

  /* Get operation mode */
  if(iw_get_ext(skfd, ifname, SIOCGIWMODE, &wrq) >= 0)
    {
      info->b.mode = wrq.u.mode;
      if((info->b.mode < IW_NUM_OPER_MODE) && (info->b.mode >= 0))
	info->b.has_mode = 1;
    }

  /* Get Power Management settings */
  wrq.u.power.flags = 0;
  if(iw_get_ext(skfd, ifname, SIOCGIWPOWER, &wrq) >= 0)
    {
      info->has_power = 1;
      memcpy(&(info->power), &(wrq.u.power), sizeof(iwparam));
    }

#if WIRELESS_EXT > 9
  /* Get Transmit Power */
  if(iw_get_ext(skfd, ifname, SIOCGIWTXPOW, &wrq) >= 0)
    {
      info->has_txpower = 1;
      memcpy(&(info->txpower), &(wrq.u.txpower), sizeof(iwparam));
    }
#endif

#if WIRELESS_EXT > 10
  /* Get retry limit/lifetime */
  if(iw_get_ext(skfd, ifname, SIOCGIWRETRY, &wrq) >= 0)
    {
      info->has_retry = 1;
      memcpy(&(info->retry), &(wrq.u.retry), sizeof(iwparam));
    }
#endif	/* WIRELESS_EXT > 10 */

  /* Get stats */
  if(iw_get_stats(skfd, ifname, &(info->stats), &(info->range), info->has_range) >= 0)
    {
      info->has_stats = 1;
    }

  return(0);
}

static PyObject *
get_iwconfig (PyObject * self, PyObject * args)
{
  char *devname;
  int skfd;		/* generic raw socket desc.	*/
  char buffer[1024];
  int eno = 0;
  struct wireless_info	info;
  PyObject * dict;
    
  if (!PyArg_ParseTuple(args, "s", &devname)) {
    return NULL;
  }

  /* Create a channel to the NET kernel. */
  if((skfd = iw_sockets_open()) < 0) {
    eno = errno;
    sprintf(buffer, "iw_sockets_open [Errno %d] %s", eno, strerror(eno));
    PyErr_SetString(PyExc_IOError, buffer);
    return NULL;
  }

  eno = get_info(skfd, devname, &info);

  close(skfd);

  if (eno < 0) {
    eno = -eno;
    sprintf(buffer, "get_info [Errno %d] %s", eno, strerror(eno));
    PyErr_SetString(PyExc_IOError, buffer);
    return NULL;
  }

  dict = PyDict_New();
  if (info.b.has_mode)
    PyDict_SetItemString(dict, "Mode", 
                         PyString_FromString(iw_operation_mode[info.b.mode]));
  if (info.b.essid_on) {
    PyDict_SetItemString(dict, "ESSID", PyString_FromString(info.b.essid));
  } else {
    PyDict_SetItemString(dict, "ESSID", PyString_FromString("Auto"));
  }
  if (info.b.has_nwid) {
    if(info.b.nwid.disabled)
      PyDict_SetItemString(dict, "NWID", PyString_FromString("Auto"));
    else
      PyDict_SetItemString(dict, "NWID", PyString_FromFormat("%X", info.b.nwid.value));
  }

  /* Display frequency / channel */
  if(info.b.has_freq)
    {
      iw_print_freq_value(buffer, sizeof(buffer), info.b.freq);
      PyDict_SetItemString(dict, "Frequency", PyString_FromString(buffer));
    }

  /* Display the address of the current Access Point */
  if(info.has_ap_addr)
    {
      iw_saether_ntop((struct sockaddr *)info.ap_addr.sa_data, buffer);
      /* Oups ! No Access Point in Ad-Hoc mode */
      if((info.b.has_mode) && (info.b.mode == IW_MODE_ADHOC))
	PyDict_SetItemString(dict, "Cell", PyString_FromString(buffer));
      else
	PyDict_SetItemString(dict, "Access Point", PyString_FromString(buffer));
    }

  /* Display the currently used/set bit-rate */
  if(info.has_bitrate)
    {
      /* Display it */
      iw_print_bitrate(buffer, sizeof(buffer), info.bitrate.value);
      PyDict_SetItemString(dict, "BitRate", PyString_FromString(buffer));
/*       printf("Bit Rate%c%s   ", (info.bitrate.fixed ? '=' : ':'), buffer); */
    }

  /* Display encryption information */
  /* Note : we display only the "current" key, use iwlist to list all keys */
  if(info.b.has_key)
    {
      if((info.b.key_flags & IW_ENCODE_DISABLED) || (info.b.key_size == 0))
	PyDict_SetItemString(dict, "Key", PyString_FromString("off"));
      else
	{
	  /* Display the key */
	  iw_print_key(buffer, sizeof(buffer), info.b.key, info.b.key_size, info.b.key_flags);
	PyDict_SetItemString(dict, "Key", PyString_FromString(buffer));

  	} 
    }

  return(dict);

}


static struct PyMethodDef PyEthModuleMethods[] = {
    	{ "get_iwconfig",
          (PyCFunction) get_iwconfig, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL }	
};

void initiwscan(void) {
  PyObject *m, *d;
  
  m = Py_InitModule("iwscan", PyEthModuleMethods);
  d = PyModule_GetDict(m);
}