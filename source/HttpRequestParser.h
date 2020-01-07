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

#ifndef _HTTP_RESPONSE_PARSER_H_
#define _HTTP_RESPONSE_PARSER_H_

#include "http_parser.h"
#include "HttpParsedRequest.h"

class HttpRequestParser {
public:

    HttpRequestParser(HttpParsedRequest* parsedRequest, Callback<void(const char *at, uint32_t length)> bodyCallback = 0)
        : _parsedRequest(parsedRequest), _bodyCallback(bodyCallback)
    {
        _parser_type = HTTP_REQUEST; 

        _settings = new http_parser_settings();

        _settings->on_message_begin = &HttpRequestParser::on_message_begin_callback;
        _settings->on_url = &HttpRequestParser::on_url_callback;
        _settings->on_status = &HttpRequestParser::on_status_callback;
        _settings->on_header_field = &HttpRequestParser::on_header_field_callback;
        _settings->on_header_value = &HttpRequestParser::on_header_value_callback;
        _settings->on_headers_complete = &HttpRequestParser::on_headers_complete_callback;
        _settings->on_chunk_header = &HttpRequestParser::on_chunk_header_callback;
        _settings->on_chunk_complete = &HttpRequestParser::on_chunk_complete_callback;
        _settings->on_body = &HttpRequestParser::on_body_callback;
        _settings->on_message_complete = &HttpRequestParser::on_message_complete_callback;

        // Construct the http_parser object
        _parser = new http_parser();
        http_parser_init(_parser, _parser_type);
        _parser->data = (void*)this;
    }

    ~HttpRequestParser() {
        if (_parser) {
            delete _parser;
        }
        if (_settings) {
            delete _settings;
        }
    }

    void clear() {
        http_parser_init(_parser, _parser_type);
    }

    uint32_t execute(const char* buffer, uint32_t buffer_size) {
        return http_parser_execute(_parser, _settings, buffer, buffer_size);
    }

    void finish() {
        http_parser_execute(_parser, _settings, NULL, 0);
    }

private:
    // Member functions
    int on_message_begin(http_parser* parser) {
        return 0;
    }

    int on_url(http_parser* parser, const char *at, uint32_t length) {
        string s(at, length);
        _parsedRequest->set_url(s);
        return 0;
    }

    int on_status(http_parser* parser, const char *at, uint32_t length) {
        string s(at, length);
        _parsedRequest->set_status(parser->status_code, s);
        return 0;
    }

    int on_header_field(http_parser* parser, const char *at, uint32_t length) {
        string s(at, length);
        _parsedRequest->set_header_field(s);
        return 0;
    }

    int on_header_value(http_parser* parser, const char *at, uint32_t length) {
        string s(at, length);
        _parsedRequest->set_header_value(s);
        return 0;
    }

    int on_headers_complete(http_parser* parser) {
        _parsedRequest->set_headers_complete();
        _parsedRequest->set_method((http_method)parser->method);
        _parsedRequest->set_Upgrade(parser->upgrade);
        return 0;
    }

    int on_body(http_parser* parser, const char *at, uint32_t length) {
        _parsedRequest->increase_body_length(length);

        if (_bodyCallback) {
            _bodyCallback(at, length);
            return 0;
        }

        _parsedRequest->set_body(at, length);
        return 0;
    }

    int on_message_complete(http_parser* parser) {
        _parsedRequest->set_message_complete(parser);

        return 0;
    }

    int on_chunk_header(http_parser* parser) {
        _parsedRequest->set_chunked();

        return 0;
    }

    int on_chunk_complete(http_parser* parser) {
        return 0;
    }

    // Static http_parser callback functions
    static int on_message_begin_callback(http_parser* parser) {
        return ((HttpRequestParser*)parser->data)->on_message_begin(parser);
    }

    static int on_url_callback(http_parser* parser, const char *at, uint32_t length) {
        return ((HttpRequestParser*)parser->data)->on_url(parser, at, length);
    }

    static int on_status_callback(http_parser* parser, const char *at, uint32_t length) {
        return ((HttpRequestParser*)parser->data)->on_status(parser, at, length);
    }

    static int on_header_field_callback(http_parser* parser, const char *at, uint32_t length) {
        return ((HttpRequestParser*)parser->data)->on_header_field(parser, at, length);
    }

    static int on_header_value_callback(http_parser* parser, const char *at, uint32_t length) {
        return ((HttpRequestParser*)parser->data)->on_header_value(parser, at, length);
    }

    static int on_headers_complete_callback(http_parser* parser) {
        return ((HttpRequestParser*)parser->data)->on_headers_complete(parser);
    }

    static int on_body_callback(http_parser* parser, const char *at, uint32_t length) {
        return ((HttpRequestParser*)parser->data)->on_body(parser, at, length);
    }

    static int on_message_complete_callback(http_parser* parser) {
        return ((HttpRequestParser*)parser->data)->on_message_complete(parser);
    }

    static int on_chunk_header_callback(http_parser* parser) {
        return ((HttpRequestParser*)parser->data)->on_chunk_header(parser);
    }

    static int on_chunk_complete_callback(http_parser* parser) {
        return ((HttpRequestParser*)parser->data)->on_chunk_complete(parser);
    }

    HttpParsedRequest* _parsedRequest;
    Callback<void(const char *at, uint32_t length)> _bodyCallback;
    http_parser* _parser;
    http_parser_type _parser_type;
    http_parser_settings* _settings;
};

#endif // _HTTP_RESPONSE_PARSER_H_
