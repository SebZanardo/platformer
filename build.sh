# The simplest possible build command (raylib->GNU Linux)
# https://github.com/raysan5/raylib/wiki/Working-on-GNU-Linux
cc src/main.c -lraylib -lGL -lm -lpthread -ldl -lrt -lX11 -o game.exe

# Exit the script if the last command was unsuccessful
if [ $? -ne 0 ]; then
	exit 1
fi

# Run the executable that was created
./game.exe
