# Building PulseView GUI with FX2G3 Support

[PulseView](https://sigrok.org/wiki/PulseView) is a Qt based logic analyzer, oscilloscope and MSO GUI for sigrok.

The EZ-USB&trade; FX2G3 Logic Analyzer implementation is consistent with the sigrok interfaces and support for the same can be added in the PulseView GUI application.

General instructions for building PulseView binaries for Linux from source are provided in the [sigrok wiki](https://sigrok.org/wiki/Linux#Building_(manually)).

At the stage of building libsigrok from source, the patches provided in `libsigrok-fx2g3.patch` need to be applied before running the autogen.sh, configure, make and make install commands. The steps for compilation of the libsigrok library with FX2G3 addition are as follows:

- git clone https://github.com/sigrokproject/libsigrok.git
- cd libsigrok
- git apply ../mtb-example-fx2g3-logic-analyzer/sigrok_build/libsigrok-fx2g3.patch
- ./autogen.sh
- ./configure
- make
- sudo make install

Once libsigrok with FX2G3 support has been compiled and installed, you can proceed with compilation of the other libraries and the PulseView GUI as documented in the sigrok wiki.

