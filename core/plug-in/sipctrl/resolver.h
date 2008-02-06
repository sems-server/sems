#ifndef _resolver_h_
#define _resolver_h_

struct sockaddr_storage;

/* struct name_entry */
/* { */
/*     string name; */
/*     string address; */
/* }; */

enum address_type {

    IPv4=1,
    IPv6=2
};

enum proto_type {
    
    TCP=1,
    UDP=2
};

class resolver
{
    static resolver* _instance;

    resolver();
    ~resolver();

 public:

    static resolver* instance();
    
    int resolve_name(const char* name, sockaddr_storage* sa, 
		     const address_type types, const proto_type protos);
};


#endif
