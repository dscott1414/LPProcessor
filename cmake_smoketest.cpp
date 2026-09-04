// B0 toolchain smoke test — NOT part of lpcore/lp/CorpusAnalysis. Proves the
// macOS build environment (compiler, CMake, dependency discovery) works
// end to end before any real port code is written. Safe to delete once the
// real lpcore target builds and links successfully (tracked in the port plan).
#include <cstdio>
#include <string>

#ifdef LP_SMOKETEST_HAVE_CURL
#include <curl/curl.h>
#endif
#ifdef LP_SMOKETEST_HAVE_JNI
#include <jni.h>
#endif
#include "yajl_tree.h"

int main()
{
    printf("lp_smoketest: C++ standard = %ld\n", __cplusplus);

    std::u16string u16 = u"portable wide string works";
    printf("lp_smoketest: u16string length = %zu\n", u16.size());

    const char* json = "{\"ok\":true}";
    char errbuf[256];
    yajl_val node = yajl_tree_parse(json, errbuf, sizeof(errbuf));
    printf("lp_smoketest: yajl parse %s\n", node ? "OK" : "FAILED");
    if (node) yajl_tree_free(node);

#ifdef LP_SMOKETEST_HAVE_CURL
    printf("lp_smoketest: curl version = %s\n", curl_version());
#else
    printf("lp_smoketest: curl NOT linked\n");
#endif

#ifdef LP_SMOKETEST_HAVE_JNI
    printf("lp_smoketest: JNI headers found (jint size = %zu)\n", sizeof(jint));
#else
    printf("lp_smoketest: JNI NOT found\n");
#endif

    return node ? 0 : 1;
}
