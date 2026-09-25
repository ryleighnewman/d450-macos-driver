rastertolabeltspl: rastertolabeltspl.c
	cc -O2 -Wall -Wno-deprecated-declarations -arch arm64 -arch x86_64 -mmacosx-version-min=11.0 -o $@ $< -lcups
