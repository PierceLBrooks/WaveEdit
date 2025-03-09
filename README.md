# Synthesis Technology WaveEdit

The wavetable and bank editor for the Synthesis Technology [E370](http://synthtech.com/eurorack/E370/) and [E352](http://synthtech.com/eurorack/E352/) Eurorack synthesizer modules.

## Building

Clone the in-source dependencies.

```
	git submodule update --init --recursive
```

Compile the program.

### Windows

```
	mkdir cmake
	cd cmake
	cmake -G "MinGW Makefiles" -DCMAKE_INSTALL_PREFIX=../install -DCMAKE_BUILD_TYPE=Release ..
	cmake --build . --target install --config Release --verbose
	cd ..
```

### Linux

```
	mkdir cmake
	cd cmake
	cmake -G "Unix Makefiles" -DCMAKE_INSTALL_PREFIX=../install -DCMAKE_BUILD_TYPE=Release ..
	cmake --build . --target install --config Release --verbose
	cd ..
```

### MacOS

```
	mkdir cmake
	cd cmake
	cmake -G "Xcode" -DCMAKE_INSTALL_PREFIX=../install -DCMAKE_BUILD_TYPE=Release ..
	cmake --build . --target install --config Release --verbose
	cd ..
```

Launch the program.

```
	./WaveEdit
```
