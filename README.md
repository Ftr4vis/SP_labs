# SP_labs
Laboratory works on the course "System Programming"

# Install
To compile source files use: \
make all

# Usage:
./lab1 <short_options> <long_options_from_plugins> <dir>
At the same time you can use your own plugins.

# Usage examples:
./lab1 --same-bytes 5 --same-bytes-comp eq ./ \
./lab1 -v \
./lab1 -h \
./lab1 -P ./plugindir/ --same-bytes 5 --same-bytes-comp eq ./ \
./lab1 -N -P ./plugindir/ --same-bytes 5 --same-bytes-comp eq ./

# Uninstall
To delete binary files use: \
make clean
 
