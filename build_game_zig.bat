zig build-exe -target wasm32-freestanding -O ReleaseSmall -fno-entry --export-table -rdynamic game.zig -femit-bin=game.wasm
