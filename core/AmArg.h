#ifndef _AmArg_h_
#define _AmArg_h_

#include <assert.h>

#include <vector>
using std::vector;

class AmArg
{
public:
    // type enum
    enum {
	Undef=0,

	Int,
	Double,
	CStr
    };

private:
    // type
    short type;
    
    // value
    union {
	
	int         v_int;
	double      v_double;
	const char* v_cstr;
    };

public:
    AmArg(const AmArg& v)
	: type(v.type){
	
	switch(type){
	case Int: v_int = v.v_int; break;
	case Double: v_double = v.v_double; break;
	case CStr: v_cstr = v.v_cstr; break;
	default: assert(0);
	}
    }

    AmArg(const int& v)
	: type(Int),
	  v_int(v)
    {}

    AmArg(const double& v)
	: type(Double),
	  v_double(v)
    {}

    AmArg(const char* v)
	: type(CStr),
	  v_cstr(v)
    {}

    short getType() const { return type; }

    int         asInt()    const { return v_int; }
    double      asDouble() const { return v_double; }
    const char* asCStr()   const { return v_cstr; }
};

class AmArgArray
{
    vector<AmArg> v;

public:
    AmArgArray() : v() {}
    AmArgArray(const AmArgArray& a) : v(a.v) {}
    
    void push(const AmArg& a){
	v.push_back(a);
    }

    const AmArg& get(size_t idx) const {
	
	assert(idx < v.size());
	return v[idx];
    }

    size_t size() { return v.size(); }
};

#endif
