
 Intel IPP G.729 codec wrapper
 -----------------------------

 Copyright (c) 2007, Vadim Lebedev
 Copyright (c) 2010, Stefan Sayer

 This is a wrapper for Intel IPP 0.6 G729 codec.

 How to build g729 module
 ------------------------

 1. Get and install Intel IPP from:
 http://software.intel.com/en-us/intel-ipp/
 (at least 3G disk space required)

 2. Get IPP Code Samples (including USC) from
 http://software.intel.com/en-us/articles/intel-integrated-performance-primitives-code-samples/
 Extract (preferably to /opt/intel/ipp/ipp-samples)

 3. Build USC library
 run build script in ipp-samples/speech-codecs

 4. adapt paths in Makefile (especially IPP_SAMPLES_PATH/IPP_SAMPLES_ARCH)
 or pass paths to make as in
 $ IPP_SAMPLES_PATH=/path/to/ipp-samples/ \
   IPP_SAMPLES_ARCH=linux64_gcc4 \
   IPPROOT=/path/to/ipp/version/arch \
   make

 About Licensing
 ---------------

 There is two distinct parts to G.729 licensing: The software licensing
 and the codec (patents) licensing.

 A. Software licensing

 1. The g729 codec wrapper code is licensed under simplified BSD license (see g729.c).

 2. Intel IPP license is required to use Intel IPP software.

 3. SEMS is licensed under GPL; the Intel IPP license is not a GPL compatible
    license. Under the GPL, you may use (modify, compile, link, run, etc) SEMS code
    included by the g729 module, but the GPL does NOT allow redistribution of
    binaries linked with code licensed under the IPP license terms.

    If you are interested in distributing SEMS binaries with the g729 module
    (g729.so), please contact info@iptel.org to obtain a commercial license (see 
    doc/COPYING for details).

 B. Codec licensing

 To use G.729 codec, in many countries you need to get a license to use the
 patents (at least until 2016 or so). The G.729 patent pool is managed by
 Sipro: http://www.sipro.com/
