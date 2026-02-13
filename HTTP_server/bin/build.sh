#!/bin/bash

g++ -o $1 ../$1.cpp -std=c++20 -O2  -lboost_system
