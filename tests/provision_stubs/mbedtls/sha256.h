#pragma once
#include <openssl/evp.h>
struct mbedtls_sha256_context{EVP_MD_CTX* p;};
inline void mbedtls_sha256_init(mbedtls_sha256_context*c){c->p=EVP_MD_CTX_new();}
inline void mbedtls_sha256_free(mbedtls_sha256_context*c){EVP_MD_CTX_free(c->p);}
inline int mbedtls_sha256_starts_ret(mbedtls_sha256_context*c,int){return EVP_DigestInit_ex(c->p,EVP_sha256(),nullptr)==1?0:-1;}
inline int mbedtls_sha256_update_ret(mbedtls_sha256_context*c,const unsigned char*p,size_t n){return EVP_DigestUpdate(c->p,p,n)==1?0:-1;}
inline int mbedtls_sha256_finish_ret(mbedtls_sha256_context*c,unsigned char*p){unsigned n=0;return EVP_DigestFinal_ex(c->p,p,&n)==1 && n==32?0:-1;}
