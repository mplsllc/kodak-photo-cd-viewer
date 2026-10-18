# pcdDecode

`pcdDecode.cpp` and `pcdDecode.h` are from Sandy McGuffog's pcdDecode 1.0.10 source, mirrored at <https://github.com/kmaragos/pcdtojpeg>. The source headers state GNU GPL version 2 or (at the user's option) any later version; the complete GPL-2 text is in `COPYING`.

The library is used in-process to read Photo CD files and return sRGB pixel buffers. No separate converter is required. Our integration requests the selected resolution and uses the decoder's orientation-corrected output. Optional 64Base/IPE support is not enabled in this first slice.

Local source SHA-256:

- `pcdDecode.cpp`: `4e4d50602a7c33465802f6662f3c3365becc4ebb120d3885bb72d489d068527c`
- `pcdDecode.h`: `f242b40cc00e30039ff3c03d510fbf82a958432fc42d2b66742693031497dbb5`
