/*
 * Copyright 2017-2018 Baidu Inc.
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

#include "openrasp_hook.h"
#include "openrasp_v8.h"

/**
 * fileupload相关hook点
 */
PRE_HOOK_FUNCTION(move_uploaded_file, fileUpload);

void pre_global_move_uploaded_file_fileUpload(OPENRASP_INTERNAL_FUNCTION_PARAMETERS)
{
    zval **name, **dest;
    int argc = MIN(2, ZEND_NUM_ARGS());
    if (argc == 2 &&
        SG(rfc1867_uploaded_files) != NULL &&
        zend_get_parameters_ex(argc, &name, &dest) == SUCCESS &&
        Z_TYPE_PP(name) == IS_STRING &&
        Z_TYPE_PP(dest) == IS_STRING &&
        zend_hash_exists(SG(rfc1867_uploaded_files), Z_STRVAL_PP(name), Z_STRLEN_PP(name) + 1) &&
        (PG(http_globals)[TRACK_VARS_FILES] || zend_is_auto_global(ZEND_STRL("_FILES") TSRMLS_CC)))
    {
        zval **realname = nullptr;
        HashTable *ht = Z_ARRVAL_P(PG(http_globals)[TRACK_VARS_FILES]);
        for (zend_hash_internal_pointer_reset(ht);
             zend_hash_has_more_elements(ht) == SUCCESS;
             zend_hash_move_forward(ht))
        {
            zval **file, **tmp_name;
            if (zend_hash_get_current_data(ht, (void **)&file) != SUCCESS ||
                Z_TYPE_PP(file) != IS_ARRAY ||
                zend_hash_find(Z_ARRVAL_PP(file), ZEND_STRS("tmp_name"), (void **)&tmp_name) != SUCCESS ||
                Z_TYPE_PP(tmp_name) != IS_STRING ||
                zend_binary_strcmp(Z_STRVAL_PP(tmp_name), Z_STRLEN_PP(tmp_name), Z_STRVAL_PP(name), Z_STRLEN_PP(name)) != 0)
            {
                continue;
            }
            if (zend_hash_find(Z_ARRVAL_PP(file), ZEND_STRS("name"), (void **)&realname) == SUCCESS)
            {
                break;
            }
        }
        if (!realname)
        {
            realname = dest;
        }
        php_stream *stream = php_stream_open_wrapper(Z_STRVAL_PP(name), "rb", 0, NULL);
        if (stream)
        {
            char *contents;
            int len = php_stream_copy_to_mem(stream, &contents, 4 * 1024, 0);
            php_stream_close(stream);
            if (len > 0)
            {
                v8::Isolate *isolate = openrasp::get_isolate(TSRMLS_C);
                if (!isolate)
                {
                    return;
                }
                bool is_block = false;
                {
                    v8::HandleScope handle_scope(isolate);
                    auto params = v8::Object::New(isolate);
                    params->Set(openrasp::NewV8String(isolate, "filename"), openrasp::NewV8String(isolate, Z_STRVAL_PP(realname), Z_STRLEN_PP(realname)));
                    params->Set(openrasp::NewV8String(isolate, "content"), openrasp::NewV8String(isolate, contents, MIN(len, 4 * 1024)));
                    efree(contents);
                    is_block = openrasp::openrasp_check(isolate, openrasp::NewV8String(isolate, check_type), params TSRMLS_CC);
                }
                if (is_block)
                {
                    handle_block(TSRMLS_C);
                }
            }
        }
    }
}