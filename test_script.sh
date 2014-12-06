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


make compile_1
make compile_3
make compile_5

if [ "$POLICY" = "1" ]
then
    testFIFO
fi