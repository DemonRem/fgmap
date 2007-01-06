
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>

#include <simgear/math/SGMath.hxx>


#define QS              "QUERY_STRING"
#define HTTP_HOST       "HTTP_HOST"
//#define DOCUMENT_URI    "DOCUMENT_URI"
#define SCRIPT_NAME    "SCRIPT_NAME"

#define XML_HEADER "Pragma: no-cache\r\nCache-Control: no-cache\r\nExpires: Sat, 17 Sep 1977 00:00:00 GMT\r\nContent-Type: text/xml\r\n\r\n"

#define FG_SERVER_KML           "fg_server_kml"

#define KML_HEADER "Pragma: no-cache\r\nCache-Control: no-cache\r\nExpires: Sat, 17 Sep 1977 00:00:00 GMT\r\nContent-Type: application/vnd.google-earth.kml+xml\r\n\r\n"

// XXX: scary macro
#define KML_DAE_FMTSTR "http://%s/%s/ge/daes/%s/%s.dae", http_host, document_path


/* From FG */
#define MAX_CALLSIGN_LEN        8
#define MAX_MODEL_NAME_LEN      96

#define DEFAULT_MODEL           "c172p"
//#define DEFAULT_MODEL           "ufo"
//#define DEFAULT_MODEL           "737-300"


static const char *models[] =
{
    "c172p", "ufo", "737-300",
};


static void do_xml_header(int);
static void do_xml_single(char *, char *, char *,
        float, float, float, float, float, float);
static void do_xml_tail();

static void do_kml_header(int);
static void do_kml_single(char *, char *, char *,
        float, float, float, float, float, float);
static void do_kml_tail();

static int callsign_cnt = 0;
static char **callsigns = NULL;
static char *callsign_buf = NULL;

static char http_host[256] = "";
static char document_uri[256] = "";
static char document_path[256] = "";
static char fg_server_port[256] = "";

static void do_kml_update_header(int);
static void do_kml_update_single(char *, char *, char *,
        float, float, float, float, float, float);
static void do_kml_update_tail();

struct output_funcs
{
    void (*header_func) (int);
    void (*single_func) (char *callsign, char *server_ip, char *model_file,
            float lat, float lon, float alt,
            float heading, float roll, float pitch);
    void (*tail_func) (void);
};

static const struct output_funcs xml_funcs =
{ do_xml_header, do_xml_single, do_xml_tail };

static const struct output_funcs kml_funcs =
{ do_kml_header, do_kml_single, do_kml_tail };

static const struct output_funcs kml_update_funcs =
{ do_kml_update_header, do_kml_update_single, do_kml_update_tail };


#if 1
static void
euler_get(float lat, float lon, float ox, float oy, float oz,
        float *heading, float *pitch, float *roll)
{
    /* FGMultiplayMgr::ProcessPosMsg */

    SGVec3f angleAxis;
    angleAxis(0) = ox;
    angleAxis(1) = oy;
    angleAxis(2) = oz;

    SGQuatf ecOrient;
    ecOrient = SGQuatf::fromAngleAxis(angleAxis);

    /* FGAIMultiplayer::update */

    float lat_rad, lon_rad;
    lat_rad = lat * SGD_DEGREES_TO_RADIANS;
    lon_rad = lon * SGD_DEGREES_TO_RADIANS;

    SGQuatf qEc2Hl = SGQuatf::fromLonLatRad(lon_rad, lat_rad);

    SGQuatf hlOr = conj(qEc2Hl) * ecOrient;

    float hDeg, pDeg, rDeg;
    hlOr.getEulerDeg(hDeg, pDeg, rDeg);

    if(heading)
        *heading = hDeg;
    if(pitch)
        *pitch = pDeg;
    if(roll)
        *roll = rDeg;

}
#else
static void
euler_get(float x, float y, float z, float ox, float oy, float oz,
        float *heading, float *pitch, float *roll)
{
    /* FGMultiplayMgr::ProcessPosMsg */

    SGVec3d ecPos;
    ecPos(0) = x;
    ecPos(1) = y;
    ecPos(2) = z;

    SGVec3f angleAxis;
    angleAxis(0) = ox;
    angleAxis(1) = oy;
    angleAxis(2) = oz;

    SGQuatf ecOrient;
    ecOrient = SGQuatf::fromAngleAxis(angleAxis);


    /* FGAIMultiplayer::update */

    SGGeod pos = SGGeod::fromCart(ecPos);

    SGQuatf qEc2Hl = SGQuatf::fromLonLatRad((float) pos.getLongitudeRad(),
            (float) pos.getLatitudeRad());

    SGQuatf hlOr = conj(qEc2Hl) * ecOrient;

    float hDeg, pDeg, rDeg;
    hlOr.getEulerDeg(hDeg, pDeg, rDeg);

    if(heading)
        *heading = hDeg;
    if(pitch)
        *pitch = pDeg;
    if(roll)
        *roll = rDeg;

}
#endif




static const char *
model_get(const char *model)
{
    int n;
    int m = sizeof(models) / sizeof(models[0]);

    for(n = 0; n < m; n++)
    {
        if(strcmp(model, models[n]) == 0)
        {
            return model;
        }
    }

    return DEFAULT_MODEL;
}


int
main(int argc, char **argv)
{
    struct output_funcs ocs = xml_funcs;

    int s;
    char *env = NULL;
    char host[256] = "";
    int port = 0;
    char in_callsigns[256] = "";
    char *p;

    int fd = -1;
    FILE *f;

    struct hostent *h;
    struct in_addr ia;
    struct sockaddr_in sin;

    char buf[256];

    int npilots = 0;

    char callsign[MAX_CALLSIGN_LEN];
    char model[MAX_MODEL_NAME_LEN];
    char server_ip[16];
    float x, y, z;
    float lat, lon, alt;
    float ox, oy, oz;
    float heading, pitch, roll;


#if 0
    if(1)
    {
        int n;
        lat = 37.613545;
        lon = -122.357237;

        srand(time(NULL));

        ocs.header_func(0);
        for(n = 0; n < 10; n++)
        {
            lat += (n * 0.01);
            lon += (n * 0.01);
            sprintf(buf, "testpilot%d", n + 1);
            ocs.single_func(buf, "testi", "testmodel",
                    lat, lon, (n + 1) * 100, 1 + rand() % 360, 0.0, 0.0);
        }
        ocs.tail_func();
    }
    return 0;
#endif


    if((env = getenv(HTTP_HOST)) == NULL)
    {
        fprintf(stderr, "%s: HTTP_HOST\n", argv[0]);
        return -1;
    }

    strncpy(http_host, env, sizeof(http_host));

    if((env = getenv(SCRIPT_NAME)) == NULL)
    {
        fprintf(stderr, "%s: SCRIPT_NAME\n", argv[0]);
        return -1;
    }

    strncpy(document_uri, env, sizeof(document_uri));

    if(env[0] == '/')
    {
        env++;
    }
    strncpy(document_path, env, sizeof(document_path));
    if((p = rindex(document_path, '/')))
    {
        *p = '\0';
    }

    if((env = getenv(QS)) == NULL)
    {
        fprintf(stderr, "%s: QUERY_STRING\n", argv[0]);
        return -1;
    }

    s = sscanf(env, "%255[^:]:%d&callsigns=%255s", host, &port, in_callsigns);

    if(s < 2)
    {
        return -1;
    }

    if(s == 3 || strstr(env, "&callsigns="))
    {
        ocs = kml_update_funcs;

        for(p = strtok(in_callsigns, ":"); p; p = strtok(NULL, ":"))
        {
            callsigns = (char **)
                realloc(callsigns, (callsign_cnt + 1) * sizeof(char *));
            callsigns[callsign_cnt++] = strdup(p);
        }
    }
    else if(strstr(argv[0], FG_SERVER_KML))
    {
        ocs = kml_funcs;
    }

    if(!host || strlen(host) == 0)
    {
        return -1;
    }

    if(port <= 0)
    {
        return -1;
    }

    h = gethostbyname(host);

    if(h == NULL)
    {
        ocs.header_func(0);
        ocs.tail_func();
        return 0;
    }

    snprintf(fg_server_port, sizeof(fg_server_port), "%s:%d", host, port);
    
    errno = 0;
    fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

    if(fd < 0)
    {
        return -1;
    }


    memcpy(&ia, h->h_addr_list[0], h->h_length);
    sin.sin_family = AF_INET;
    sin.sin_addr = ia;
    sin.sin_port = htons(port);

    if(connect(fd, (struct sockaddr *) &sin, sizeof(sin)) < 0)
    {
        close(fd);
        ocs.header_func(0);
        ocs.tail_func();
        return 0;
    }

    f = fdopen(fd, "r");

    while(!feof(f))
    {
        int s;

        if(fgets(buf, sizeof(buf), f) == NULL)
        {
            break;
        }

        strtok(buf, "\n");

        if(buf[0] == '#')
        {
            if(sscanf(buf, "# %d pilots(s) online", &npilots) == 1)
            {
                ocs.header_func(npilots);
            }
        }
        else if((s = sscanf(buf,
                        "%7[^@]@%15[^:]: %f %f %f %f %f %f %f %f %f %95s",
                    callsign, server_ip,
                    &x, &y, &z,
                    &lat, &lon, &alt,
                    &ox, &oy, &oz,
                    model) == 12))
        {
            char *model_file = NULL;

            //euler_get(x, y, z, ox, oy, oz, &heading, &roll, &pitch);
            euler_get(lat, lon, ox, oy, oz, &heading, &roll, &pitch);

            /* Get the filename part from the model path */
            if((model_file = rindex(model, '/')))
            {
                model_file += 1;
                if(*model_file)
                {
                    strtok(model_file, ".");
                }
            }

            p = callsign;

            while(*p)
            {
                if(*p == '"')
                {
                    /* Damn, quick hack */
                    *p = '_';
                }
                p++;
            }

            ocs.single_func(callsign, server_ip, model_file,
                    lat, lon, alt, heading, roll, pitch);
        }
        /*
        else
        {
            fprintf(stderr, "buf[%s][%d]\n", buf, s);
        }
        */
    }

    ocs.tail_func();

    close(fd);

    return 0;
}


/* xml */
static void
do_xml_header(int npilots)
{
    printf(XML_HEADER);
    printf("<fg_server pilot_cnt=\"%d\">\n", npilots);
}


static void
do_xml_single(char *callsign, char *server_ip, char *model_file,
        float lat, float lon, float alt,
        float heading, float roll, float pitch)
{
    printf("\t<marker callsign=\"%s\" server_ip=\"%s\" model=\"%s\" lat=\"%f\" lng=\"%f\" alt=\"%f\" heading=\"%f\" roll=\"%f\" pitch=\"%f\" />\n",
            callsign, server_ip, model_file,
            lat, lon, alt,
            heading, roll, pitch);
}


static void
do_xml_tail()
{
    printf("</fg_server>\n\n");
}


/* kml */
static void
do_kml_header(int npilots)
{
    callsigns = (char **) calloc(npilots, sizeof(char *));

    printf(KML_HEADER);

    printf("<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n\
<kml xmlns=\"http://earth.google.com/kml/2.0\">\n\
<Document id=\"mpmap\">\n\
<name>FlightGear MP server map</name>\n\
<visibility>1</visibility>\n");
}


static void
do_kml_single(char *callsign, char *server_ip, char *model_file,
        float lat, float lon, float alt,
        float heading, float roll, float pitch)
{
    char buf[BUFSIZ];
    const char *model;

    callsigns[callsign_cnt++] = strdup(callsign);

    alt *= 0.3048;      /* feet to meter */

    /* TODO */
    model = model_get(model_file);
    snprintf(buf, sizeof(buf), KML_DAE_FMTSTR, model, model);

    printf("\n\
    <Placemark id=\"%s\">\n\
        <name>%s</name>\n\
        <description>%s: %s</description>\n\
        <Model>\n\
            <altitudeMode>absolute</altitudeMode>\n\
            <Location>\n\
                <latitude>%f</latitude>\n\
                <longitude>%f</longitude>\n\
                <altitude>%f</altitude>\n\
            </Location>\n\
            <Orientation>\n\
                <heading>%f</heading>\n\
                <roll>%f</roll>\n\
                <tilt>%f</tilt>\n\
            </Orientation>\n\
            <Scale>\n\
                <x>150.0</x>\n\
                <y>150.0</y>\n\
                <z>150.0</z>\n\
            </Scale>\n\
            <Link>\n\
                <href>%s</href>\n\
                <refreshMode>onChange</refreshMode>\n\
            </Link>\n\
        </Model>\n\
    </Placemark>\n\
",
        callsign, callsign, callsign, model_file,
        lat, lon, alt,
        heading, roll, pitch,
        buf);

}


static void
do_kml_tail()
{
    int n;

    printf("\n\
    <NetworkLink id=\"fgmap_update\">\n\
        <name>Update</name>\n\
        <Link>\n\
            <href>http://%s/%s/fg_server_kml.cgi?%s&amp;callsigns=",
            http_host, document_path, fg_server_port);

    for(n = 0; n < callsign_cnt; n++)
    {
        printf("%s:", callsigns[n]);
        free(callsigns[n]);
        callsigns[n] = NULL;
    }
    
    printf("</href>\n");

    printf("\
            <refreshMode>onInterval</refreshMode>\n\
            <refreshInterval>5</refreshInterval>\n");

    printf("\
        </Link>\n\
    </NetworkLink>\n");

    printf("</Document>\n</kml>\n");

    free(callsigns);
    callsigns = NULL;
}



/* kml update */
static void
do_kml_update_header(int npilots)
{
    printf(KML_HEADER);

    printf("<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n\
<kml xmlns=\"http://earth.google.com/kml/2.0\">\n\
<NetworkLinkControl>\n\
<Update>\n\
    <targetHref>http://%s/%s/fg_server_kml.cgi?%s</targetHref>",
        http_host, document_path, fg_server_port);

    if(npilots > 0)
    {
        callsign_buf =
            (char *) calloc(npilots * MAX_CALLSIGN_LEN, sizeof(char));
    }
}



static void
do_kml_update_single(char *callsign, char *server_ip, char *model_file,
        float lat, float lon, float alt,
        float heading, float roll, float pitch)
{
    int n, found = 0;
    char buf[BUFSIZ];

    alt *= 0.3048;      /* feet to meter */

    for(n = 0; n < callsign_cnt; n++)
    {
        if(callsigns[n] && strcmp(callsigns[n], callsign) == 0)
        {
            free(callsigns[n]);
            callsigns[n] = NULL;
            found = 1;
            break;
        }
    }

    if(callsign_buf)
    {
        strcat(callsign_buf, callsign);
        strcat(callsign_buf, ":");
    }

    if(found)
    {
        printf("\n\
    <Change>\n\
        <Placemark targetId=\"%s\">\n\
            <Model>\n\
                <Location>\n\
                    <latitude>%f</latitude>\n\
                    <longitude>%f</longitude>\n\
                    <altitude>%f</altitude>\n\
                </Location>\n\
                <Orientation>\n\
                    <heading>%f</heading>\n\
                    <roll>%f</roll>\n\
                    <tilt>%f</tilt>\n\
                </Orientation>\n\
            </Model>\n\
        </Placemark>\n\
    </Change>\n\
",
            callsign,
            lat, lon, alt,
            heading, roll, pitch);
    }
    else
    {
        const char *model;

        model = model_get(model_file);

        /* TODO */
        snprintf(buf, sizeof(buf), KML_DAE_FMTSTR, model, model);

        printf("\n\
    <Create>\n\
        <Document targetId=\"mpmap\">\n\
            <Placemark id=\"%s\">\n\
                <name>%s</name>\n\
                <description>%s: %s</description>\n\
                <Model>\n\
                    <altitudeMode>absolute</altitudeMode>\n\
                    <Location>\n\
                        <latitude>%f</latitude>\n\
                        <longitude>%f</longitude>\n\
                        <altitude>%f</altitude>\n\
                    </Location>\n\
                    <Orientation>\n\
                        <heading>%f</heading>\n\
                        <roll>%f</roll>\n\
                        <tilt>%f</tilt>\n\
                    </Orientation>\n\
                    <Scale>\n\
                        <x>1000.0</x>\n\
                        <y>1000.0</y>\n\
                        <z>1000.0</z>\n\
                    </Scale>\n\
                    <Link>\n\
                        <href>%s</href>\n\
                        <refreshMode>onChange</refreshMode>\n\
                    </Link>\n\
                </Model>\n\
            </Placemark>\n\
        </Document>\n\
    </Create>\n\
",
            callsign, callsign, callsign, model_file,
            lat, lon, alt,
            heading, roll, pitch,
            buf);
    }

}


static void
do_kml_update_tail()
{
    int n;

    for(n = 0; n < callsign_cnt; n++)
    {
        if(callsigns[n] != NULL)
        {
            printf("\n\
    <Delete>\n\
        <Placemark targetId=\"%s\" />\n\
    </Delete>\n\
", callsigns[n]);

            free(callsigns[n]);
            callsigns[n] = NULL;
        }
    }

    printf("\n\
    <Change>\n\
        <NetworkLink targetId=\"fgmap_update\">\n\
            <Link>\n\
                <href>http://%s/%s/fg_server_kml.cgi?%s&amp;callsigns=%s</href>\n\
            </Link>\n\
        </NetworkLink>\n\
    </Change>\n",
        http_host, document_path, fg_server_port,
        (callsign_buf ? callsign_buf : ""));

    printf("\n</Update>\n</NetworkLinkControl>\n");

    printf("</kml>\n");

    free(callsigns);
    callsigns = NULL;
    free(callsign_buf);
}

