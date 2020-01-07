/*
 * PackageLicenseDeclared: Apache-2.0
 * Copyright (c) 2017 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _MBED_HTTP_HTTP_RESPONSE
#define _MBED_HTTP_HTTP_RESPONSE
#include <string>
#include <map>
#include "http_parser.h"

using namespace std;

typedef map<string, string>  MapHeaders;
typedef MapHeaders::iterator MapHeaderIterator;

class HttpParsedRequest {
public:
    HttpParsedRequest() {
        body = NULL;
        clear();
    }

    ~HttpParsedRequest() {
       clear();
    }

    void clear() {
        status_code = 0;
        concat_header_field = false;
        concat_header_value = false;
        expected_content_length = 0;
        is_chunked = false;
        is_message_completed = false;
        body_length = 0;
        body_offset = 0;
        if (body != NULL) {
            free(body);
            body = NULL;
        }
        
        headers.clear();
    }

    void set_status(int a_status_code, string a_status_message) {
        status_code = a_status_code;
        status_message = a_status_message;
    }

    int get_status_code() {
        return status_code;
    }

    string get_status_message() {
        return status_message;
    }

    void set_url(string a_url) {
        url = a_url;
    }

    string get_url() {
        return url;
    }

    /*
        return path up to last /
    */
    string get_path() {
        string path(url);
        size_t found = url.find_last_of("/");
        if(found) {
            path.erase(found+1, string::npos);
        }
        return path;
    }

    /*
        return filename from last / to first ?
    */
    string get_filename() {
        string filename;
        size_t foundSlash = url.find_last_of("/");
        if (foundSlash == string::npos)
            foundSlash = 0;
        size_t foundQM = url.find_first_of("?");
        if (foundQM)
            foundQM--;

        filename = url.substr(foundSlash+1, foundQM - foundSlash);
        return filename;
    }

    /*
        return filename from first ? to end
    */
    string get_query() {
        string query;
        size_t foundQM = url.find_first_of("?");
        if (foundQM == string::npos)
            return query;

        query = url.substr(foundQM);
        return query;
    }

    void set_method(http_method a_method) {
        method = a_method;
    }
 
    http_method get_method() {
        return method;
    }

    void set_Upgrade(bool a_Upgrade) {
        is_Upgrade = a_Upgrade;
    }

    bool get_Upgrade() {
        return is_Upgrade;
    }

    MapHeaders headers;

    void set_header_field(string field) {
        concat_header_value = false;

        // headers can be chunked
        if (concat_header_field) {
            _headerField += field;
        }
        else {
            _headerField = field;
        }

        concat_header_field = true;
    }

    void set_header_value(string value) {
        concat_header_field = false;

        // headers can be chunked
        if (concat_header_value) {
            _headerValue += value;
        }
        else {
            _headerValue = value;
            headers[_headerField] = _headerValue;
        }

        concat_header_value = true;
    }

    // called by parser on request
    void set_headers_complete() {
        MapHeaderIterator it = headers.find("content-length");
        if(it != headers.end()) {
            expected_content_length = atoi(it->second.c_str());
        }
    }

    // called by parser on request
    void set_body(const char *at, uint32_t length) {
        // Connection: close, could not specify Content-Length, nor chunked... So do it like this:
        if (expected_content_length == 0 && length > 0) {
            is_chunked = true;
        }

        // only malloc when this fn is called, so we don't alloc when body callback's are enabled
        if (body == NULL && !is_chunked) {
            body = (char*)malloc(expected_content_length);
        }

        if (is_chunked) {
            if (body == NULL) {
                body = (char*)malloc(length);
            }
            else {
                char* original_body = body;
                body = (char*)realloc(body, body_offset + length);
                if (body == NULL) {
                    free(original_body);
                    return;
                }
            }
        }

        memcpy(body + body_offset, at, length);

        body_offset += length;
    }

    void* get_body() {
        return (void*)body;
    }

    string get_body_as_string() {
        string s(body, body_offset);
        return s;
    }

    void increase_body_length(uint32_t length) {
        body_length += length;
    }

    uint32_t get_body_length() {
        return body_offset;
    }

    bool is_message_complete() {
        return is_message_completed;
    }

    void set_chunked() {
        is_chunked = true;
    }

    void set_message_complete(http_parser* parser) {
        is_message_completed = true;
        http_major = parser->http_major;
        http_minor = parser->http_minor;
    }

private:
    // from http://stackoverflow.com/questions/5820810/case-insensitive-string-comp-in-c
    int strcicmp(char const *a, char const *b) {
        for (;; a++, b++) {
            int d = tolower(*a) - tolower(*b);
            if (d != 0 || !*a) {
                return d;
            }
        }
    }

    char tolower(char c) {
        if(('A' <= c) && (c <= 'Z')) {
            return 'a' + (c - 'A');
        }

        return c;
    }

    int status_code;
    string status_message;
    string url;
    http_method method;

    string _headerField;
    string _headerValue;
    //vector<string*> header_fields;
    //vector<string*> header_values;

    bool concat_header_field;
    bool concat_header_value;

    uint32_t expected_content_length;

    bool is_chunked;
    bool is_message_completed;
    bool is_Upgrade;                // upgrade requst found
    uint16_t http_minor;
    uint16_t http_major;

    char * body;
    uint32_t body_length;
    uint32_t body_offset;
};

#endif
