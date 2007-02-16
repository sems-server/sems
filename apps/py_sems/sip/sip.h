/*
 * The SIP module interface.
 *
 * Copyright (c) 2004
 * 	Riverbank Computing Limited <info@riverbankcomputing.co.uk>
 * 
 * This file is part of SIP.
 * 
 * This copy of SIP is licensed for use under the terms of the SIP License
 * Agreement.  See the file LICENSE for more details.
 * 
 * SIP is supplied WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */


#ifndef _SIP_H
#define	_SIP_H


#include <Python.h>

/*
 * There is a mis-feature somewhere with the Borland compiler.  This works
 * around it.
 */
#if defined(__BORLANDC__)
#include <rpc.h>
#endif


#ifdef __cplusplus
extern "C" {
#endif


/* Sanity check on the Python version. */
#if PY_VERSION_HEX < 0x02030000
#error "This version of SIP requires Python v2.3 or later"
#endif


/*
 * Define the SIP version number.
 */
#define	SIP_VERSION		0x040101
#define	SIP_VERSION_STR		"4.1.1"
#define	SIP_BUILD		"255"


/*
 * Define the current API version number.  SIP must handle modules with the
 * same major number and with the same or earlier minor number.  Whenever data
 * structure elements are added they must be appended and the minor number
 * incremented.  Whenever data structure elements are removed or the order
 * changed then the major number must be incremented and the minor number set
 * to 0.
 *
 * History:
 *
 * 0.2	Added the 'H' format character to sip_api_parse_args().
 *
 * 0.1	Added sip_api_add_class_instance().
 *	Added the 't' format character to sip_api_parse_args().
 *	Deprecated the 'J' and 'K' format characters to sip_api_parse_result().
 *
 * 0.0	Original version.
 */
#define	SIP_API_MAJOR_NR	0
#define	SIP_API_MINOR_NR	2


/* Some compatibility stuff to help with handwritten code for SIP v3. */
#if !defined(ANY)
#define ANY     void
#endif


/*
 * The mask that can be passed to sipTrace().
 */
#define	SIP_TRACE_CATCHERS	0x0001
#define	SIP_TRACE_CTORS		0x0002
#define	SIP_TRACE_DTORS		0x0004
#define	SIP_TRACE_INITS		0x0008
#define	SIP_TRACE_DEALLOCS	0x0010
#define	SIP_TRACE_METHODS	0x0020


/*
 * Hide some thread dependent stuff.
 */
#ifdef WITH_THREAD
typedef PyGILState_STATE sip_gilstate_t;
#define	SIP_RELEASE_GIL(gs)	PyGILState_Release(gs);
#define	SIP_BLOCK_THREADS	{PyGILState_STATE sipGIL = PyGILState_Ensure();
#define	SIP_UNBLOCK_THREADS	PyGILState_Release(sipGIL);}
#else
typedef int sip_gilstate_t;
#define	SIP_RELEASE_GIL(gs)
#define	SIP_BLOCK_THREADS
#define	SIP_UNBLOCK_THREADS
#endif


/*
 * The metatype for a wrapper type.
 */
typedef struct _sipWrapperType {
	PyHeapTypeObject super;		/* The super-metatype. */
	struct _sipTypeDef *type;	/* The additional type information. */
} sipWrapperType;


/*
 * A C/C++ object wrapped as a Python object.
 */
typedef struct _sipWrapper {
	PyObject_HEAD
	union {
		void *cppPtr;		/* C/C++ object pointer. */
		void *(*afPtr)();	/* Access function. */
	} u;
	int flags;			/* Object flags. */
	PyObject *dict;			/* The instance dictionary. */
	struct _sipPySig *pySigList;	/* Python signal list (complex). */
	struct _sipWrapper *next;	/* Next object at this address. */
} sipWrapper;


/*
 * Some convenient function pointers.
 */
typedef void *(*sipInitFunc)(sipWrapper *,PyObject *,int *);
typedef void (*sipDeallocFunc)(sipWrapper *);
typedef void *(*sipCastFunc)(void *,sipWrapperType *);
typedef sipWrapperType *(*sipSubClassConvertFunc)(void *);
typedef void *(*sipForceConvertToFunc)(PyObject *,int *);
typedef int (*sipConvertToFunc)(PyObject *,void **,int *);
typedef PyObject *(*sipConvertFromFunc)(void *);
typedef struct _sipProxy *(*sipProxyFunc)(void);
typedef int (*sipVirtHandlerFunc)(void *,PyObject *,...);
typedef int (*sipEmitFunc)(sipWrapper *,PyObject *);


/*
 * The information describing an enum value instance to be added to a
 * dictionary.
 */
typedef struct _sipEnumValueInstanceDef {
	char *evi_name;			/* The enum value name. */
	int evi_val;			/* The enum value value. */
} sipEnumValueInstanceDef;


/*
 * The information describing static instances.
 */
typedef struct _sipInstancesDef {
	struct _sipClassInstanceDef *id_class;	/* The classes. */
	struct _sipVoidPtrInstanceDef *id_voidp;	/* The void *. */
	struct _sipCharInstanceDef *id_char;	/* The chars. */
	struct _sipStringInstanceDef *id_string;	/* The strings. */
	struct _sipLongInstanceDef *id_long;	/* The longs. */
	struct _sipDoubleInstanceDef *id_double;	/* The doubles. */
} sipInstancesDef;


/*
 * The information describing a super-class.
 */
typedef struct _sipSuperClassDef {
	unsigned sc_class:16;		/* The class number. */
	unsigned sc_module:8;		/* The module number (255 for this one). */
	unsigned sc_last:1;		/* The last in the list. */
} sipSuperClassDef;


/*
 * The information describing a sub-class convertor.
 */
typedef struct _sipSubClassConvertorDef {
	sipSubClassConvertFunc scc_convertor;	/* The convertor. */
	sipSuperClassDef scc_base;	/* The encoded base type. */
	sipWrapperType *scc_basetype;	/* The base type. */
} sipSubClassConvertorDef;


/*
 * The different Python slot types.
 */
typedef enum {
	str_slot,			/* __str__ */
	int_slot,			/* __int__ */
	len_slot,			/* __len__ */
	contains_slot,			/* __contains__ */
	add_slot,			/* __add__ for number */
	concat_slot,			/* __add__ for sequence types */
	sub_slot,			/* __sub__ */
	mul_slot,			/* __mul__ for number types */
	repeat_slot,			/* __mul__ for sequence types */
	div_slot,			/* __div__ */
	mod_slot,			/* __mod__ */
	and_slot,			/* __and__ */
	or_slot,			/* __or__ */
	xor_slot,			/* __xor__ */
	lshift_slot,			/* __lshift__ */
	rshift_slot,			/* __rshift__ */
	iadd_slot,			/* __iadd__ for number types */
	iconcat_slot,			/* __iadd__ for sequence types */
	isub_slot,			/* __isub__ */
	imul_slot,			/* __imul__ for number types */
	irepeat_slot,			/* __imul__ for sequence types */
	idiv_slot,			/* __idiv__ */
	imod_slot,			/* __imod__ */
	iand_slot,			/* __iand__ */
	ior_slot,			/* __ior__ */
	ixor_slot,			/* __ixor__ */
	ilshift_slot,			/* __ilshift__ */
	irshift_slot,			/* __irshift__ */
	invert_slot,			/* __invert__ */
	call_slot,			/* __call__ */
	getitem_slot,			/* __getitem__ */
	setitem_slot,			/* __setitem__ */
	delitem_slot,			/* __delitem__ */
	lt_slot,			/* __lt__ */
	le_slot,			/* __le__ */
	eq_slot,			/* __eq__ */
	ne_slot,			/* __ne__ */
	gt_slot,			/* __gt__ */
	ge_slot,			/* __ge__ */
	cmp_slot,			/* __cmp__ */
	nonzero_slot,			/* __nonzero__ */
	neg_slot,			/* __neg__ */
	repr_slot			/* __repr__ */
} sipPySlotType;


/*
 * The information describing a Python slot function.
 */
typedef struct _sipPySlotDef {
	void *psd_func;			/* The function. */
	sipPySlotType psd_type;		/* The type. */
} sipPySlotDef;


/*
 * The information describing a type.
 */
typedef struct _sipTypeDef {
	struct _sipExportedModuleDef *td_module;	/* The module. */
	char *td_name;			/* The name of the type. */
	int td_scope;			/* The nr. of the scoping type. */
	sipSuperClassDef *td_supers;	/* The super-types. */
	sipPySlotDef *td_pyslots;	/* The table of Python slots. */
	int td_nrmethods;		/* The number of lazy methods. */
	PyMethodDef *td_methods;	/* The table of lazy methods. */
	int td_nrenums;			/* The number of lazy enums. */
	sipEnumValueInstanceDef *td_enums;	/* The table of lazy enums. */
	PyMethodDef *td_variables;	/* The variable table. */
	sipInitFunc td_init;		/* The initialisation function. */
	sipDeallocFunc td_dealloc;	/* The deallocation function. */
	sipCastFunc td_cast;		/* The cast function, 0 if a C struct. */
	sipForceConvertToFunc td_fcto;	/* The force convert to function, 0 if a C++ namespace. */
	sipConvertToFunc td_cto;	/* The convert to function. */
	sipProxyFunc td_proxy;		/* The create proxy function. */
	struct _sipQtSignal *td_emit;	/* Emit table for Qt signals. */
	sipInstancesDef td_instances;	/* The static instances. */
} sipTypeDef;


/*
 * The information describing a mapped class.
 */
typedef struct _sipMappedTypeDef {
	sipForceConvertToFunc mt_fcto;	/* The force convert to function. */
	sipConvertToFunc mt_cto;	/* The convert to function. */
	sipConvertFromFunc mt_cfrom;	/* The convert from function. */
} sipMappedTypeDef;


/*
 * The information describing an imported module.
 */
typedef struct _sipImportedModuleDef {
	char *im_name;			/* The module name. */
	int im_version;			/* The required version. */
	struct _sipExportedModuleDef *im_module;	/* The imported module. */
} sipImportedModuleDef;


/*
 * The main client module structure.
 */
typedef struct _sipExportedModuleDef {
	struct _sipExportedModuleDef *em_next;	/* The next in the list. */
	char *em_name;			/* The module name. */
	int em_version;			/* The module version. */
	sipImportedModuleDef *em_imports;	/* The imported modules. */
	int em_qobject_class;		/* The index of the QObject class. */
	struct _sipWrapperType **em_types;	/* The table of types. */
	sipMappedTypeDef **em_mappedtypes;	/* The table of mapped types. */
	sipVirtHandlerFunc *em_virthandlers;	/* The table of virtual handlers. */
	sipSubClassConvertorDef *em_convertors;	/* The sub-class convertors. */
	sipInstancesDef em_instances;	/* The static instances. */
	struct _sipLicenseDef *em_license;	/* The license. */
} sipExportedModuleDef;


/*
 * The information describing a license to be added to a dictionary.
 */
typedef struct _sipLicenseDef {
	char *lc_type;			/* The type of license. */
	char *lc_licensee;		/* The licensee. */
	char *lc_timestamp;		/* The timestamp. */
	char *lc_signature;		/* The signature. */
} sipLicenseDef;


/*
 * The information describing a void pointer instance to be added to a
 * dictionary.
 */
typedef struct _sipVoidPtrInstanceDef {
	char *vi_name;			/* The void pointer name. */
	void *vi_val;			/* The void pointer value. */
} sipVoidPtrInstanceDef;


/*
 * The information describing a char instance to be added to a dictionary.
 */
typedef struct _sipCharInstanceDef {
	char *ci_name;			/* The char name. */
	char ci_val;			/* The char value. */
} sipCharInstanceDef;


/*
 * The information describing a string instance to be added to a dictionary.
 */
typedef struct _sipStringInstanceDef {
	char *si_name;			/* The string name. */
	char *si_val;			/* The string value. */
} sipStringInstanceDef;


/*
 * The information describing a long instance to be added to a dictionary.
 */
typedef struct _sipLongInstanceDef {
	char *li_name;			/* The long name. */
	long li_val;			/* The long value. */
} sipLongInstanceDef;


/*
 * The information describing a double instance to be added to a dictionary.
 */
typedef struct _sipDoubleInstanceDef {
	char *di_name;			/* The double name. */
	double di_val;			/* The double value. */
} sipDoubleInstanceDef;


/*
 * The information describing a class instance to be added to a dictionary.
 */
typedef struct _sipClassInstanceDef {
	char *ci_name;			/* The class name. */
	void *ci_ptr;			/* The actual instance. */
	struct _sipWrapperType **ci_type;	/* A pointer to the Python type. */
	int ci_flags;			/* The wrapping flags. */
} sipClassInstanceDef;


/*
 * Define a mapping between a wrapped type identified by a string and the
 * corresponding Python type.
 */
typedef struct _sipStringTypeClassMap {
	char *typeString;		/* The type as a string. */
	struct _sipWrapperType **pyType;	/* A pointer to the Python type. */
} sipStringTypeClassMap;


/*
 * Define a mapping between a wrapped type identified by an integer and the
 * corresponding Python type.
 */
typedef struct _sipIntTypeClassMap {
	int typeInt;			/* The type as an integer. */
	struct _sipWrapperType **pyType;	/* A pointer to the Python type. */
} sipIntTypeClassMap;


/*
 * A Python method's component parts.  This allows us to re-create the method
 * without changing the reference counts of the components.
 */
typedef struct _sipPyMethod {
	PyObject *mfunc;		/* The function. */
	PyObject *mself;		/* Self if it is a bound method. */
	PyObject *mclass;		/* The class. */
} sipPyMethod;


/*
 * Cache a reference to a Python member function.
 */
typedef struct _sipMethodCache {
	int mcflags;			/* Method cache flags. */
	sipPyMethod pyMethod;		/* The method. */
} sipMethodCache;


/*
 * A slot (in the Qt, rather than Python, sense).
 */
typedef struct _sipSlot {
	char *name;			/* Name if a Qt or Python signal. */
	PyObject *pyobj;		/* Signal or Qt slot object. */
	sipPyMethod meth;		/* Python slot method, pyobj is NULL. */
	PyObject *weakSlot;		/* A weak reference to the slot. */
} sipSlot;


/*
 * A proxy slot.
 */
typedef struct _sipProxy {
	void *qproxy;			/* The proxy QObject. */
	const char **slotTable;		/* The table of slots. */
	sipSlot realSlot;		/* The Python slot. */
	struct _sipWrapper *txSelf;	/* The transmitter. */
	char *txSig;			/* The transmitting signal. */
	const char *rxSlot;		/* The receiving slot. */
	struct _sipProxy *next;		/* Next in list. */
	struct _sipProxy *prev;		/* Previous in list. */
} sipProxy;


/*
 * A receiver of a Python signal.
 */
typedef struct _sipPySigRx {
	sipSlot rx;			/* The receiver. */
	struct _sipPySigRx *next;	/* Next in the list. */
} sipPySigRx;


/*
 * A Python signal.
 */
typedef struct _sipPySig {
	char *name;			/* The name of the signal. */
	sipPySigRx *rxlist;		/* The list of receivers. */
	struct _sipPySig *next;		/* Next in the list. */
} sipPySig;


/*
 * Maps the name of a Qt signal to a wrapper function to emit it.
 */
typedef struct _sipQtSignal {
	char *st_name;			/* The signal name. */
	sipEmitFunc st_emitfunc;	/* The emitter function. */
} sipQtSignal;


/*
 * The API exported by the SIP module, ie. pointers to all the data and
 * functions that can be used by generated code.
 */
typedef struct _sipAPIDef {
	/*
	 * The following are part of the public API.
	 */
	void (*api_bad_catcher_result)(PyObject *method);
	void (*api_bad_length_for_slice)(int seqlen,int slicelen);
	PyObject *(*api_build_result)(int *isErr,char *fmt,...);
	PyObject *(*api_call_method)(int *isErr,PyObject *method,char *fmt,...);
	PyObject *(*api_class_name)(PyObject *self);
	PyObject *(*api_connect_rx)(PyObject *txObj,const char *sig,PyObject *rxObj,const char *slot);
	int (*api_convert_from_sequence_index)(int idx,int len);
	void *(*api_convert_to_cpp)(PyObject *sipSelf,sipWrapperType *type,int *iserrp);
	PyObject *(*api_disconnect_rx)(PyObject *txObj,const char *sig,PyObject *rxObj,const char *slot);
	int (*api_emit_signal)(PyObject *self,const char *sig,PyObject *sigargs);
	void (*api_free)(void *mem);
	void *(*api_get_sender)(void);
	PyObject *(*api_get_wrapper)(void *cppPtr,sipWrapperType *type);
	void *(*api_malloc)(size_t nbytes);
	sipWrapperType *(*api_map_int_to_class)(int typeInt,const sipIntTypeClassMap *map,int maplen);
	sipWrapperType *(*api_map_string_to_class)(const char *typeString,const sipStringTypeClassMap *map,int maplen);
	int (*api_parse_result)(int *isErr,PyObject *method,PyObject *res,char *fmt,...);
	void (*api_trace)(unsigned mask,const char *fmt,...);
	void (*api_transfer)(PyObject *self,int toCpp);
	/*
	 * The following are not part of the public API.
	 */
	int (*api_export_module)(sipExportedModuleDef *client,unsigned api_major,unsigned api_minor,PyObject *mod_dict);
	int (*api_add_enum_instances)(PyObject *dict,sipEnumValueInstanceDef *evi);
	int (*api_parse_args)(int *argsParsedp,PyObject *sipArgs,char *fmt,...);
	void (*api_common_ctor)(sipMethodCache *cache,int nrmeths);
	void (*api_common_dtor)(sipWrapper *sipSelf);
	PyObject *(*api_convert_from_void_ptr)(void *val);
	void *(*api_convert_to_void_ptr)(PyObject *obj);
	void (*api_no_ctor)(int argsParsed,char *classname);
	void (*api_no_function)(int argsParsed,char *func);
	void (*api_no_method)(int argsParsed,char *classname,char *method);
	void (*api_bad_class)(const char *classname);
	void (*api_bad_set_type)(const char *classname,const char *var);
	void *(*api_get_cpp_ptr)(sipWrapper *w,sipWrapperType *type);
	void *(*api_get_complex_cpp_ptr)(sipWrapper *w);
	PyObject *(*api_is_py_method)(sip_gilstate_t *gil,sipMethodCache *pymc,sipWrapper *sipSelf,char *cname,char *mname);
	PyObject *(*api_map_cpp_to_self)(void *cppPtr,sipWrapperType *type);
	PyObject *(*api_map_cpp_to_self_sub_class)(void *cppPtr,sipWrapperType *type);
	PyObject *(*api_new_cpp_to_self)(void *cppPtr,sipWrapperType *type,int initflags);
	PyObject *(*api_new_cpp_to_self_sub_class)(void *cppPtr,sipWrapperType *type,int initflags);
	void (*api_call_hook)(char *hookname);
	void (*api_start_thread)(void);
	void (*api_end_thread)(void);
	void (*api_emit_to_slot)(void *sender,sipSlot *slot,char *fmt,...);
	void (*api_raise_unknown_exception)(void);
	void (*api_raise_class_exception)(sipWrapperType *type,void *ptr);
	void (*api_raise_sub_class_exception)(sipWrapperType *type,void *ptr);
	int (*api_add_class_instance)(PyObject *dict,char *name,void *cppPtr,sipWrapperType *wt);
} sipAPIDef;


/*
 * Useful macros, not part of the public API.
 */
#define	SIP_PY_OWNED	0x01		/* Owned by Python. */
#define	SIP_SIMPLE	0x02		/* If the instance is simple. */
#define	SIP_INDIRECT	0x04		/* If there is a level of indirection. */
#define	SIP_ACCFUNC	0x08		/* If there is an access function. */
#define	SIP_XTRA_REF	0x10		/* If C++ has an extra reference. */
#define	SIP_NOT_IN_MAP	0x20		/* If Python object not in the map. */

#define	sipIsPyOwned(w)		((w) -> flags & SIP_PY_OWNED)
#define	sipSetPyOwned(w)	((w) -> flags |= SIP_PY_OWNED)
#define	sipResetPyOwned(w)	((w) -> flags &= ~SIP_PY_OWNED)
#define	sipIsSimple(w)		((w) -> flags & SIP_SIMPLE)
#define	sipIsIndirect(w)	((w) -> flags & SIP_INDIRECT)
#define	sipIsAccessFunc(w)	((w) -> flags & SIP_ACCFUNC)
#define	sipIsExtraRef(w)	((w) -> flags & SIP_XTRA_REF)
#define	sipSetIsExtraRef(w)	((w) -> flags |= SIP_XTRA_REF)
#define	sipResetIsExtraRef(w)	((w) -> flags &= ~SIP_XTRA_REF)
#define	sipNotInMap(w)		((w) -> flags & SIP_NOT_IN_MAP)


#ifdef __cplusplus
}
#endif


#endif
