注: 这里rtmp库因简化未选用https://github.com/ossrs/srs-librtmp
通常情况下我们只会用rtmp协议测试直播流，很少直接测试rtmps等需要加密的协议，为了加快开发速度可以简单的编译librtmp不用依赖openssl和zlib。
笔者使用的linux环境编译，windows下的和这个差不多已有其他开发者记录了编译过程。
 1、首先修改rtmp_sys.h文件。
     在文件中找到#ifdef _WIN32在其前面增加一句#define NO_CRYPTO，同时找到/* USE_OPENSSL */这个注释，修改为#elif defined(USE_OPENSSL)或者直接注释掉#else的内容。
2、修改Makefile文件。
     这里主要去除编译时对openssl和zlib动态库的依赖，在Makefile中找到$(CC) $(SO_LDFLAGS) $(LDFLAGS) -o $@ $^ $> $(CRYPTO_LIB)这一行把最后的$(CRYPTO_LIB)删除。
这样就可以编译一个没有依赖的rtmp库，需要注意的是这样编译出来的库只能使用rtmp协议，如果你需要加密协议这个方法就不能用了。