#!/bin/bash

# Initialize parameters
starvation=0
stall_avoidance=0
stall_detection=0
num_philosophers=5

# Ask for user input
while true; do
    echo "Enter number of philosophers (between 4 and 256):"
    read num_philosophers
    if [[ "$num_philosophers" =~ ^[4-9]$|^[1-9][0-9]$|^1[0-9]{2}$|^2[0-4][0-9]$|^25[0-6]$ ]]; then
        break
    else
        echo "Invalid input. Please enter a number between 4 and 256."
    fi
done
while true; do
    echo "Enable Stall Detection? (1 for yes, 0 for no)"
    read stall_detection
    if [[ "$stall_detection" =~ ^[01]$ ]]; then
        break
    else
        echo "Invalid input. Please enter 1 for yes, 0 for no."
    fi
done
while true; do
    echo "Enable Stall Avoidance? (1 for yes, 0 for no)"
    read stall_avoidance
    if [[ "$stall_avoidance" =~ ^[01]$ ]]; then
        break
    else
        echo "Invalid input. Please enter 1 for yes, 0 for no."
    fi
done
while true; do
    echo "Enable Starvation Detection? (1 for yes, 0 for no)"
    read starvation
    if [[ "$starvation" =~ ^[01]$ ]]; then
        break
    else
        echo "Invalid input. Please enter 1 for yes, 0 for no."
    fi
done




# Compile the C program
gcc main.c -o main -lpthread

# Run the program with the specified arguments
./main $num_philosophers $stall_detection $stall_avoidance $starvation
