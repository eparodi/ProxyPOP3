#ifndef TPE_PROTOS_METRICS_H
#define TPE_PROTOS_METRICS_H

struct metrics {
    unsigned int concurrent_connections;
    unsigned int historical_access;
    long long int transferred_bytes;
    unsigned int retrieved_messages;
};

typedef struct metrics * metrics;

extern metrics metricas;


#endif //TPE_PROTOS_METRICS_H
