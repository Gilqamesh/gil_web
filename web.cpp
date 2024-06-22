#include "web.h"

#include <iostream>
#include <assert.h>

typedef struct async_fetch {
    std::function<void(const unsigned char* serialized_data, size_t received_data_size)> on_success;
    std::function<void()> on_failure;
} async_fetch_t;

static std::string url_combine(const std::string& scheme, const std::string& host_name, size_t port, const std::string& path_name);
static int  fetch_url_desktop(const std::string& scheme, const std::string& host_name, size_t port, const std::string& path_name, async_fetch_t* result);
static void fetch_url_async(const std::string& scheme, const std::string& host_name, size_t port, const std::string& path_name, async_fetch_t* result);
static int  fetch_url_sync(const std::string& scheme, const std::string& host_name, size_t port, const std::string& path_name, async_fetch_t* result);

static std::string url_combine(const std::string& scheme, const std::string& host_name, size_t port, const std::string& path_name) {
    return scheme + "://" + host_name + ":" + std::to_string(port) + "/" + path_name;
}

#if defined(PLATFORM_WEB)

# include <emscripten/emscripten.h>
# include <emscripten/fetch.h>

static void on_fetch_success(emscripten_fetch_t *fetch);
static void on_fetch_error(emscripten_fetch_t *fetch);
static void fetch_url_emscripten_async(const std::string& scheme, const std::string& host_name, size_t port, const std::string& path_name, async_fetch_t* result);
static int  fetch_url_emscripten_sync(const std::string& scheme, const std::string& host_name, size_t port, const std::string& path_name, async_fetch_t* result);

static void on_fetch_success(emscripten_fetch_t *fetch) {
    std::cout << "GET succeeded '" << fetch->url << "', length: " << fetch->numBytes << std::endl;

    assert(fetch->userData);
    async_fetch_t* fetch_result = reinterpret_cast<async_fetch_t*>(fetch->userData);
    fetch_result->on_success(fetch->data, fetch->numBytes);

    delete fetch_result;

    emscripten_fetch_close(fetch);
}

static void on_fetch_error(emscripten_fetch_t *fetch) {
    std::cerr << "GET failed '" << fetch->url << "', status: " << fetch->status << ", reason: " << fetch->statusText << std::endl;

    assert(fetch->userData);
    async_fetch_t* fetch_result = reinterpret_cast<async_fetch_t*>(fetch->userData);
    fetch_result->on_failure();

    delete fetch_result;

    emscripten_fetch_close(fetch);
}

static void fetch_url_emscripten_async(const std::string& scheme, const std::string& host_name, size_t port, const std::string& path_name, async_fetch_t* result) {
    emscripten_fetch_attr_t attr;

    emscripten_fetch_attr_init(&attr);
    strcpy(attr.requestMethod, "GET");
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    attr.onsuccess = on_fetch_success;
    attr.onerror = on_fetch_error;
    attr.userData = result;

    const std::string url = url_combine(scheme, host_name, port, path_name);
    emscripten_fetch(&attr, url.c_str());
}

static int fetch_url_emscripten_sync(const std::string& scheme, const std::string& host_name, size_t port, const std::string& path_name, async_fetch_t* result) {
    int fetch_result = 0;

    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    strcpy(attr.requestMethod, "GET");
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY | EMSCRIPTEN_FETCH_SYNCHRONOUS;
    
    const std::string url = url_combine(scheme, host_name, port, path_name);
    emscripten_fetch_t* fetch = emscripten_fetch(&attr, url.c_str());
    if (!fetch) {
        fetch_result = 1;
        std::cerr << "GET emscripten_fetch failed '" << url << "'" << std::endl;
        result->on_failure();
    } else if (fetch->status == 200) {
        std::cout << "GET finished '" << fetch->url << "', length: " << fetch->numBytes << std::endl;
        result->on_success(fetch->data, fetch->numBytes);
        emscripten_fetch_close(fetch);
    } else {
        fetch_result = 1;
        std::cerr << "GET failed '" << fetch->url << "', status: " << fetch->status << ", reason: " << fetch->statusText << std::endl;
        result->on_failure();
        emscripten_fetch_close(fetch);
    }

    return fetch_result;
}

#else

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"

static int fetch_url_desktop(const std::string& scheme, const std::string& host_name, size_t port, const std::string& path_name, async_fetch_t* result) {
    int fetch_result = 0;

    httplib::Client cli(scheme + "://" + host_name + ":" + std::to_string(port));
    httplib::Result res = cli.Get("/" + path_name);
    if (!res) {
        std::cerr << "httplib::Client::Get failed '" << url_combine(scheme, host_name, port, path_name) << "'" << std::endl;
        result->on_failure();
        fetch_result = 1;
    } else if (res->status == 200) {
        std::cout << "GET succeeded '" << url_combine(scheme, host_name, port, path_name) << "', length: " << res->body.size() << std::endl;
        result->on_success(reinterpret_cast<const unsigned char*>(res->body.data()), res->body.size());
    } else {
        std::cerr << "GET failed '" << url_combine(scheme, host_name, port, path_name) << "', status: " << res->status << std::endl;
        result->on_failure();
        fetch_result = 1;
    }

    return fetch_result;
}

#endif

static void fetch_url_async(const std::string& scheme, const std::string& host_name, size_t port, const std::string& path_name, async_fetch_t* result) {
    const std::string url = url_combine(scheme, host_name, port, path_name);
    std::cout << "GET started '" << url << "'" << std::endl;

#if defined(PLATFORM_WEB)
    fetch_url_emscripten_async(scheme, host_name, port, path_name, result);
#else
    fetch_url_desktop(scheme, host_name, port, path_name, result);
#endif
}

static int fetch_url_sync(const std::string& scheme, const std::string& host_name, size_t port, const std::string& path_name, async_fetch_t* result) {
#if defined(PLATFORM_WEB)
    return fetch_url_emscripten_sync(scheme, host_name, port, path_name, result);
#else
    return fetch_url_desktop(scheme, host_name, port, path_name, result);
#endif
}

void async_fetch__create_async(
    const std::string& scheme, const std::string& host_name, size_t port, const std::string& path_name,
    const std::function<void(const unsigned char* serialized_data, size_t serialized_data_size)>& on_success,
    const std::function<void()>& on_failure
) {
    async_fetch_t* self = new async_fetch_t();

    self->on_success = on_success;
    self->on_failure = on_failure;

    fetch_url_async(scheme, host_name, port, path_name, self);
}

void async_fetch__create_sync(
    const std::string& scheme, const std::string& host_name, size_t port, const std::string& path_name,
    const std::function<void(const unsigned char* serialized_data, size_t serialized_data_size)>& on_success,
    const std::function<void()>& on_failure
) {
    assert(0 && "todo: implement");

    fetch_url_sync(scheme, host_name, port, path_name, 0);

    async_fetch_t* self = new async_fetch_t();

    self->on_success = on_success;
    self->on_failure = on_failure;

    fetch_url_async(scheme, host_name, port, path_name, self);
}
