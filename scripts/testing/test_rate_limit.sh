#!/bin/bash
# Test rate limiting by sending parallel requests

PORT=8081
NUM_REQUESTS=15
PATH_TO_REQUEST="/testfile.txt"

echo "Testing Rate Limiting on port $PORT"
echo "Sending $NUM_REQUESTS parallel requests to $PATH_TO_REQUEST..."
echo "==========================================="

# Send parallel requests
for i in $(seq 1 $NUM_REQUESTS); do
    (curl -s http://localhost:$PORT$PATH_TO_REQUEST -o /dev/null -w "Request $i: HTTP %{http_code}\n") &
done

# Wait for all background jobs to complete
wait

echo "==========================================="
echo "Test complete"
