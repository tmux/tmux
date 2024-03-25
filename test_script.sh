#!/bin/bash

cd regress
# Number of commits to check out and test, for demo purposes
NUM_COMMITS=2

sum=0
# Get the last N commits
COMMITS=$(git log --format="%H" -n $NUM_COMMITS)

# Variable to store the total number of tests run
TOTAL_TESTS=$(find . -type f -name "*.sh" | wc -l)
TOTAL_PASS_COMMIT=0
for COMMIT in $COMMITS; do
    echo "start loop"
    # Variable to store the number of passed tests
    PASSED_TESTS=0
    # Checkout the commit
    git checkout $COMMIT

    for TEST_SCRIPT in *.sh; do
        echo running $TEST_SCRIPT
        chmod +x $TEST_SCRIPT
        # Execute the test script and check if it passes
        if ./$TEST_SCRIPT; then
            PASSED_TESTS=$((PASSED_TESTS + 1))
            ((sum+=1))
        fi
    done
    # Calculate the pass rate
    PASS_RATE=$(echo "scale=2; $PASSED_TESTS / $TOTAL_TESTS * 100" | bc)
    echo "Pass rate: $PASS_RATE%"
    
    if [ "$PASSED_TESTS" -eq "$TOTAL_TESTS" ]; then
        ((TOTAL_PASS_COMMIT+=1))
    fi

done

 # Calculate the Commit Success Rate
PASS_RATE=$((TOTAL_PASS_COMMIT / NUM_COMMITS))
echo "COMMIT SUCCESS RATE: $PASS_RATE%"
average=$(echo "scale=2; $sum / ($TOTAL_TESTS * $NUM_COMMITS) * 100" | bc)
echo "AVERAGE TEST SUCCESS RATE: $average%"
