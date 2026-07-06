#ifndef _TLS_H
#define _TLS_H
#include "stdint.h"

/* I/O reseau abstraite : fournie par net.c (TCP) cote OS. */
typedef struct {
    void* ctx;
    int  (*send)(void* c, const uint8_t* b, int n);  /* envoie n octets, ret n ou <0 */
    int  (*recv)(void* c, uint8_t* b, int n);        /* lit <=n octets, ret >0 ou <=0 */
    void (*rng)(void* c, uint8_t* b, int n);         /* remplit n octets aleatoires */
} TlsIO;

/* Client TLS 1.2 (RSA + AES-128-CBC-SHA256), from scratch.
 * Fait un vrai handshake et un GET HTTPS. Le certificat est PARSE pour la
 * cle RSA mais la CHAINE N'EST PAS VALIDEE (pas de verification CA).
 * Retourne le nombre d'octets recus (>=0) ou un code d'erreur negatif. */
int tls_https_get(TlsIO* io, const char* host, const char* path, char* out, int outmax);

#endif
