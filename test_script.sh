POLICY=$1

function verify {
    file1=$1
    echo -e "\t[DIFFING] $file1 results.txt"
    if diff "$file1" "results.txt" &> /dev/null ; then
        echo -e "\t\e[1;34m[PASS]\e[0m"
    else
        echo -e "\t\e[1;31m[FAIL]\e[0m"
    fi
}

function testFIFO {
    echo "[TESTING FIFO]"

    ./test_1 > /dev/null 2>&1
    echo -e "\t[TEST #1]"
    verify output_1

    ./test_3 > /dev/null 2>&1
    echo -e "\t[TEST #3]"
    verify output_3

    ./test_5 > /dev/null 2>&1
    echo -e "\t[TEST #5] -> Check manually...diff doesn't work for some reason for 5"
    verify output_5
}

function testClock {
    echo "[TESTING CLOCK]"

    ./test_2 > /dev/null 2>&1
    echo -e "\t[TEST #2]"
    verify output_2

    ./test_4 > /dev/null 2>&1
    echo -e "\t[TEST #4]"
    verify output_4

    ./test_6 > /dev/null 2>&1
    echo -e "\t[TEST #6] -> Check manually...diff doesn't work for some reason for 6"
    verify output_6
}

make compile_1
make compile_2
make compile_3
make compile_4
make compile_5
make compile_6

if [ "$POLICY" = "1" ]
then
    testFIFO
elif [ "$POLICY" = "2" ]
then
    testClock
else
    testFIFO
    testClock
fi