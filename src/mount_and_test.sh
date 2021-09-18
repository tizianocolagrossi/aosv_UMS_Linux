#!/bin/bash
cd ./kernel_module
make
sudo insmod ums_module.ko

cd ../user_library
make
./bin/test

sudo rmmod ums_module