/*
 * $Id: messages.c,v 1.16.6.1.2.7.2.1 2005/09/27 10:40:41 didg Exp $
 *
 * Copyright (c) 1997 Adrian Sun (asun@zoology.washington.edu)
 * All Rights Reserved.  See COPYRIGHT.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <atalk/afp.h>
#include <atalk/dsi.h>
#include <atalk/util.h>
#include <atalk/logger.h>
#include "globals.h"
#include "misc.h"


#define MAXMESGSIZE 199

/* this is only used by afpd children, so it's okay. */
static char servermesg[MAXPATHLEN] = "";
static char localized_message[MAXPATHLEN] = "";

void setmessage(const char *message)
{
    strlcpy(servermesg, message, MAXMESGSIZE);
}

void readmessage(obj)
AFPObj *obj;
{
    /* Read server message from file defined as SERVERTEXT */
#ifdef SERVERTEXT
    FILE *message;
    char * filename;
    unsigned int i; 
    int rc;
    static int c;
    uid_t euid;
    u_int32_t maxmsgsize;

    maxmsgsize = (obj->proto == AFPPROTO_DSI)?MIN(MAX(((DSI*)obj->handle)->attn_quantum, MAXMESGSIZE),MAXPATHLEN):MAXMESGSIZE;

    i=0;
    /* Construct file name SERVERTEXT/message.[pid] */
    if ( NULL == (filename=(char*) malloc(sizeof(SERVERTEXT)+15)) ) {
	LOG(log_error, logtype_afpd, "readmessage: malloc: %s", strerror(errno) );
        return;
    }

    sprintf(filename, "%s/message.%d", SERVERTEXT, getpid());

#ifdef DEBUG
    LOG(log_debug, logtype_afpd, "Reading file %s ", filename);
#endif /* DEBUG */

    message=fopen(filename, "r");
    if (message==NULL) {
        LOG(log_info, logtype_afpd, "Unable to open file %s", filename);
        sprintf(filename, "%s/message", SERVERTEXT);
        message=fopen(filename, "r");
    }

    /* if either message.pid or message exists */
    if (message!=NULL) {
        /* added while loop to get characters and put in servermesg */
        while ((( c=fgetc(message)) != EOF) && (i < (maxmsgsize - 1))) {
            if ( c == '\n')  c = ' ';
            servermesg[i++] = c;
        }
        servermesg[i] = 0;

        /* cleanup */
        fclose(message);

        /* Save effective uid and switch to root to delete file. */
        /* Delete will probably fail otherwise, but let's try anyways */
        euid = geteuid();
        if (seteuid(0) < 0) {
            LOG(log_error, logtype_afpd, "Could not switch back to root: %s",
				strerror(errno));
        }

        if ( 0 < (rc = unlink(filename)) )
	    LOG(log_error, logtype_afpd, "File '%s' could not be deleted", strerror(errno));

        /* Drop privs again, failing this is very bad */
        if (seteuid(euid) < 0) {
            LOG(log_error, logtype_afpd, "Could not switch back to uid %d: %s", euid, strerror(errno));
        }

        if (rc < 0) {
            LOG(log_error, logtype_afpd, "Error deleting %s: %s", filename, strerror(rc));
        }
#ifdef DEBUG
        else {
            LOG(log_info, logtype_afpd, "Deleted %s", filename);
        }

        LOG(log_info, logtype_afpd, "Set server message to \"%s\"", servermesg);
#endif /* DEBUG */
    }
    free(filename);
#endif /* SERVERTEXT */
}

int afp_getsrvrmesg(obj, ibuf, ibuflen, rbuf, rbuflen)
AFPObj *obj;
char *ibuf, *rbuf;
int ibuflen _U_, *rbuflen;
{
    char *message;
    u_int16_t type, bitmap;
    u_int16_t msgsize;
    size_t outlen = 0;
    size_t msglen = 0;
    int utf8 = 0;

    *rbuflen = 0;

    msgsize = (obj->proto == AFPPROTO_DSI)?MAX(((DSI*)obj->handle)->attn_quantum, MAXMESGSIZE):MAXMESGSIZE;

    memcpy(&type, ibuf + 2, sizeof(type));
    memcpy(&bitmap, ibuf + 4, sizeof(bitmap));

    switch (ntohs(type)) {
    case AFPMESG_LOGIN: /* login */
        message = obj->options.loginmesg;
        break;
    case AFPMESG_SERVER: /* server */
        message = servermesg;
        break;
    default:
        return AFPERR_BITMAP;
    }

    /* output format:
     * message type:   2 bytes
     * bitmap:         2 bytes
     * message length: 1 byte ( 2 bytes for utf8)
     * message:        up to 199 bytes (dsi attn_quantum for utf8)
     */
    memcpy(rbuf, &type, sizeof(type));
    rbuf += sizeof(type);
    *rbuflen += sizeof(type);
    memcpy(rbuf, &bitmap, sizeof(bitmap));
    rbuf += sizeof(bitmap);
    *rbuflen += sizeof(bitmap);

    utf8 = ntohs(bitmap) & 2; 
    msglen = strlen(message);
    if (msglen > msgsize)
        msglen = msgsize;

    if (msglen) { 
        if ( (size_t)-1 == (outlen = convert_string(obj->options.unixcharset, utf8?CH_UTF8_MAC:obj->options.maccharset,
                                                    message, msglen, localized_message, msgsize)) )
        {
    	    memcpy(rbuf+((utf8)?2:1), message, msglen); /*FIXME*/
	    outlen = msglen;
        }
        else
        {
	    memcpy(rbuf+((utf8)?2:1), localized_message, outlen);
        }
    }
    
    if ( utf8 ) {
	/* UTF8 message, 2 byte length */
	msgsize = htons(outlen);
    	memcpy(rbuf, &msgsize, sizeof(msgsize));
	*rbuflen += sizeof(msgsize);
    }
    else {
        *rbuf = outlen;
	*rbuflen += 1;
    }
    *rbuflen += outlen;

    return AFP_OK;
}
