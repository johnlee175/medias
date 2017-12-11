#!/usr/bin/env bash

#1.1.1-dev
git clone git://git.openssl.org/openssl.git
#v1.2.0 [Don't delete it after install]
git clone https://github.com/arut/nginx-rtmp-module.git
#release-1.13.8
git clone https://github.com/nginx/nginx.git

CURR_DIR=`pwd`

cd nginx &&./auto/configure \
--add-module=${CURR_DIR}/nginx-rtmp-module \
--with-openssl=${CURR_DIR}/openssl \
--with-http_ssl_module

make && sudo make install

# add to nginx.conf BEGIN:

## more see https://github.com/arut/nginx-rtmp-module/wiki/Directives
#rtmp_auto_push on;
#
#rtmp {
#    server {
#        listen 1935;
#        chunk_size 4096;
#
#        # VOD [test rtmp://localhost/vod/{your-flv-file} on vlc]
#        application vod {
#            play /usr/local/nginx/temp/vod; #flv store here first
#        }
#
#        # HLS
#        # [ffmpeg -re -i sample.flv -vcodec copy -acodec copy -f flv rtmp://localhost/hls/mystream]
#        # [test http://localhost:8080/hls/mystream.m3u8 on vlc]
#        application hls {
#            live on;
#            hls on;
#            hls_path /tmp/hls; #m3u8 will store here
#        }
#
#        # MPEG-DASH is similar to HLS
#        application dash {
#            live on;
#            dash on;
#            dash_path /tmp/dash;
#        }
#
#        # RTMP Live
#        # [ffmpeg -re -i sample.flv -vcodec copy -acodec copy -f flv rtmp://localhost/live]
#        # [test rtmp://localhost/live on vlc]
#        application live {
#            live on;
#
#            #send NetStream.Play.PublishNotify and NetStream.Play.UnpublishNotify to subscribers.
#            publish_notify on;
#
#            #if enabled, nginx-rtmp sends NetStream.Play.Start and NetStream.Play.Stop to each subscriber
#            #every time publisher starts or stops publishing.
#            play_restart on;
#
#            # record keyframes;
#            # record_path /tmp;
#            # record_max_size 128K;
#            # record_interval 30s;
#            # record_suffix .flv;
#        }
#    }
#}
#
#http {
#    server {
#        listen      8080;
#        server_name  localhost;
#
#        # This URL provides RTMP statistics in XML
#        # [test http://localhost:8080/stat]
#        location /stat {
#            rtmp_stat all;
#            # Use this stylesheet to view XML as web page in browser
#            rtmp_stat_stylesheet stat.xsl;
#        }
#
#        location /stat.xsl {
#            # XML stylesheet to view RTMP stats.
#            # Copy stat.xsl wherever you want
#            # and put the full directory path here
#            root ${CURR_DIR}/nginx-rtmp-module;
#        }
#
#        location /hls {
#            # Serve HLS fragments
#            types {
#                application/vnd.apple.mpegurl m3u8;
#                video/mp2t ts;
#            }
#            root /tmp;
#            add_header Cache-Control no-cache;
#        }
#
#        location /dash {
#            # Serve DASH fragments
#            root /tmp;
#            add_header Cache-Control no-cache;
#        }
#    }
#}

# add to nginx.conf END;

# NOTE1: MPEG-DASH
# [cd /usr/local/nginx/html && git clone https://github.com/arut/dash.js.git && cd dash.js && git checkout live]
# [open baseline.html and modify url from http://dash.edgesuite.net/envivio/dashpr/clear/Manifest.mpd
#   to http://localhost:8080/dash/mystream.mpd]
# [open nginx.conf and add "location /dash.js { root html; }]
# [/usr/local/nginx/sbin/nginx -s reload]
# [ffmpeg -re -i sample.flv -vcodec copy -acodec copy -f flv rtmp://localhost/dash/mystream]
# [test http://localhost:8080/dash.js/baseline.html]

# NOTE2: OBS (Open Broadcaster Software)
# It is currently the hottest free open source live broadcast software in the world

# NOTE3: We use rtmp, hls, dash base on nginx-rtmp-module.
# Adobe HDS(HTTP Dynamic Streaming), Microsoft HSS(HTTP Smooth Streaming) is not support;
# crtmpserver(rtmpd) and https://github.com/ossrs/srs is not for reference, may be more better.
