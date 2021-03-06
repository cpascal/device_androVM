#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <VBox/VBoxGuest.h>
#include <iprt/err.h>
#include <VBox/HostServices/GuestPropertySvc.h>
#include <VBGLR3Internal.h>

using namespace guestProp;

int connectProp(int *clientID)
{
    VBoxGuestHGCMConnectInfo Info;
    memset(&Info, 0, sizeof(Info));
    Info.result = VERR_WRONG_ORDER;
    Info.Loc.type = VMMDevHGCMLoc_LocalHost_Existing;
    strcpy(Info.Loc.u.host.achName, "VBoxGuestPropSvc");
    Info.u32ClientID = UINT32_MAX;  /* try make valgrind shut up. */

    int iofile = open("/dev/vboxguest",O_RDWR);
    if (!iofile) {
        fprintf(stderr,"Unable to open /dev/vboxguest errno=%d\n",errno);
        return -1;
    }

    int rc = ioctl(iofile, VBOXGUEST_IOCTL_HGCM_CONNECT, &Info);
    if (rc) {
        fprintf(stderr,"Error in ioctl errno=%d\n", errno);
        return -2;
    }

    rc = Info.result;
    if (RT_FAILURE(rc)) {
        fprintf(stderr,"Connect NOK, rc=%d\n",rc);
	return -1;
    }

    *clientID = Info.u32ClientID;

    return iofile;
}

char *getProp(int fd_ioctl, int clientID, char *propname)
{
    GetProperty Msg;
    int rc;
    char *retbuf;
#define RETBUF_SIZE 1024

    retbuf = (char *)malloc(RETBUF_SIZE*sizeof(char));
    if (!retbuf)
	return NULL;

    Msg.hdr.result = VERR_WRONG_ORDER;
    Msg.hdr.u32ClientID = clientID;
    Msg.hdr.u32Function = GET_PROP;
    Msg.hdr.cParms = 4;
    VbglHGCMParmPtrSetString(&Msg.name, propname);
    VbglHGCMParmPtrSet(&Msg.buffer, retbuf, RETBUF_SIZE);
    VbglHGCMParmUInt64Set(&Msg.timestamp, 0);
    VbglHGCMParmUInt32Set(&Msg.size, 0);

    rc = ioctl(fd_ioctl, VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg);
    if (rc) {
        fprintf(stderr,"Error in ioctl errno=%d\n", errno);
        return NULL;
    }

    rc = Msg.hdr.result;

    if (RT_FAILURE(rc)) {
	fprintf(stderr, "Get error, rc=%d\n", rc);
	return NULL;
    }

    return retbuf;
}

int setProp(int fd_ioctl, int clientID, char *propname, char *value)
{
    SetProperty Msg;
    int rc;

    Msg.hdr.result = VERR_WRONG_ORDER;
    Msg.hdr.u32ClientID = clientID;
    Msg.hdr.u32Function = SET_PROP_VALUE;
    Msg.hdr.cParms = 3;
    VbglHGCMParmPtrSetString(&Msg.name, propname);
    VbglHGCMParmPtrSetString(&Msg.value, value);
    VbglHGCMParmPtrSetString(&Msg.flags, "");

    rc = ioctl(fd_ioctl, VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg);
    if (rc) {
        fprintf(stderr,"Error in ioctl errno=%d\n", errno);
        return NULL;
    }

    rc = Msg.hdr.result;
    if (RT_SUCCESS(rc)) 
	return 0;
    else
	return -1;
}

void print_usage(char *pname) 
{
    fprintf(stderr, "Usage: %s get [property_name]\n       %s set [property_name] [property_value]\n       %s rm [property_name]", pname, pname, pname);
}

int main(int argc, char *argv[]) 
{
    int fd_ioctl;
    int clientID;

    if (argc<3) {
	print_usage(argv[0]);
        return -1;
    }

    fd_ioctl = connectProp(&clientID);
    if (fd_ioctl<0) {
	fprintf(stderr, "Unable to open ioctl\n");
	return -1;
    }

    if (strcmp(argv[1],"get")==0) {
	char *val = getProp(fd_ioctl, clientID, argv[2]);
	if (!val)
	    return -1;
	printf("%s",val);
	free(val);
    }
    else if (strcmp(argv[1],"set")==0) {
	if (argc<4) {
	    print_usage(argv[0]);
	    return -1;
	}
	if (setProp(fd_ioctl, clientID, argv[2], argv[3])<0)
	    return -1;
    }
    else if (strcmp(argv[1],"rm")==0) {
	if (setProp(fd_ioctl, clientID, argv[2], "")<0)
	    return -1;
    }
    else {
	print_usage(argv[0]);
	return -1;
    }

    return 0;
}
