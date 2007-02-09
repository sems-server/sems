%MappedType string
{
%TypeHeaderCode
#include <string>
using std::string;
%End

%ConvertFromTypeCode
     const char *s = sipCpp->c_str();
     return PyString_FromString(s);
%End

%ConvertToTypeCode
         // Allow a Python string whenever a string is expected.
         if (sipIsErr == NULL)
             return PyString_Check(sipPy);
         if (sipPy == Py_None) {
             *sipCppPtr = NULL;
             return 0;
         }
         if (PyString_Check(sipPy)) {
             *sipCppPtr = new std::string(PyString_AS_STRING(sipPy));
             return 0;
         }
         *sipCppPtr = (std::string *)sipForceConvertTo_string(sipPy,sipIsErr);
         return 1;
%End
};

