#!/usr/bin/env bash
# test_tinyurl.sh — full test suite for the tinyURL server
# Usage: ./test_tinyurl.sh [host]
# Example: ./test_tinyurl.sh localhost:8080

HOST="${1:-localhost:8080}"
BASE="http://$HOST"
PASS=0
FAIL=0

# ── Colors ────────────────────────────────────────────────────────────────
GREEN='\033[0;32m'
RED='\033[0;91m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
RESET='\033[0m'

# ── Helpers ───────────────────────────────────────────────────────────────
ok()   { echo -e "  ${GREEN}✔${RESET}  $1"; ((PASS++)); }
fail() { echo -e "  ${RED}✘${RESET}  $1"; ((FAIL++)); }
info() { echo -e "  ${CYAN}→${RESET}  $1"; }
header() {
    echo ""
    echo -e "${BOLD}${YELLOW}━━━  $1  ━━━${RESET}"
}

# POST and return full response body
post() {
    curl -s -X POST "$BASE$1" \
        -H "Content-Type: application/json" \
        -d "$2"
}

# GET and return full response body
get() {
    curl -s "$BASE$1"
}

# DELETE and return HTTP status code only
del_status() {
    curl -s -o /dev/null -w "%{http_code}" -X DELETE "$BASE$1"
}

# Assert a JSON field equals a value
assert_field() {
    local label="$1" json="$2" field="$3" expected="$4"
    local actual
    actual=$(echo "$json" | grep -o "\"$field\":[^,}]*" | head -1 | sed 's/.*://;s/[" ]//g')
    if [ "$actual" = "$expected" ]; then
        ok "$label (got $field=$actual)"
    else
        fail "$label — expected $field=$expected, got $field=$actual"
        info "Full response: $json"
    fi
}

# Assert string is present anywhere in response
assert_contains() {
    local label="$1" haystack="$2" needle="$3"
    if echo "$haystack" | grep -q "$needle"; then
        ok "$label"
    else
        fail "$label — expected to find '$needle'"
        info "Full response: $haystack"
    fi
}

# Assert string is NOT present
assert_not_contains() {
    local label="$1" haystack="$2" needle="$3"
    if ! echo "$haystack" | grep -q "$needle"; then
        ok "$label"
    else
        fail "$label — did NOT expect '$needle'"
        info "Full response: $haystack"
    fi
}

assert_status() {
    local label="$1" actual="$2" expected="$3"
    if [ "$actual" = "$expected" ]; then
        ok "$label (HTTP $actual)"
    else
        fail "$label — expected HTTP $expected, got HTTP $actual"
    fi
}

# ── Check server is up ────────────────────────────────────────────────────
header "Preflight"
HEALTH=$(get /health)
if echo "$HEALTH" | grep -q '"status"'; then
    ok "Server is reachable at $BASE"
else
    echo -e "\n${RED}ERROR: Cannot reach server at $BASE${RESET}"
    echo "  Start it with: ./tinyURL"
    exit 1
fi

# ─────────────────────────────────────────────────────────────────────────
header "1 · Health check"
# ─────────────────────────────────────────────────────────────────────────

R=$(get /health)
assert_contains "GET /health returns status ok"    "$R" '"status":"ok"'
assert_contains "GET /health returns server field" "$R" '"server"'
assert_contains "GET /health returns total_urls"   "$R" '"total_urls"'
info "Response: $R"

# ─────────────────────────────────────────────────────────────────────────
header "2 · Create short URLs  (POST /)"
# ─────────────────────────────────────────────────────────────────────────

URLS=(
    "https://github.com"
    "https://google.com"
    "https://stackoverflow.com"
    "https://news.ycombinator.com"
    "https://wikipedia.org"
    "https://reddit.com"
    "https://youtube.com"
    "https://twitter.com"
    "https://anthropic.com"
    "https://apple.com"
)

declare -A CODE_MAP   # long_url → short_code

for url in "${URLS[@]}"; do
    R=$(post "/" "{\"long_url\":\"$url\"}")
    CODE=$(echo "$R" | grep -o '"short_code":"[^"]*"' | head -1 | sed 's/.*://;s/"//g')
    if [ -n "$CODE" ]; then
        ok "Created $url → $CODE"
        CODE_MAP["$url"]="$CODE"
    else
        fail "Failed to shorten $url"
        info "Response: $R"
    fi
done

# ─────────────────────────────────────────────────────────────────────────
header "3 · Custom short code  (POST / with short_code)"
# ─────────────────────────────────────────────────────────────────────────

R=$(post "/" '{"long_url":"https://example.com","short_code":"ex"}')
assert_contains "Custom code 'ex' accepted"   "$R" '"short_code":"ex"'
assert_contains "short_url includes the code" "$R" '/ex'

R2=$(post "/" '{"long_url":"https://example2.com","short_code":"ex"}')
assert_contains "Duplicate code returns 409" "$R2" 'already taken'

# ─────────────────────────────────────────────────────────────────────────
header "4 · Deduplication — same long URL returns same code"
# ─────────────────────────────────────────────────────────────────────────

R1=$(post "/" '{"long_url":"https://github.com"}')
R2=$(post "/" '{"long_url":"https://github.com"}')
CODE1=$(echo "$R1" | grep -o '"short_code":"[^"]*"' | sed 's/.*://;s/"//g')
CODE2=$(echo "$R2" | grep -o '"short_code":"[^"]*"' | sed 's/.*://;s/"//g')
if [ "$CODE1" = "$CODE2" ] && [ -n "$CODE1" ]; then
    ok "Same long_url → same short_code ($CODE1)"
else
    fail "Dedup failed: got $CODE1 and $CODE2"
fi

# ─────────────────────────────────────────────────────────────────────────
header "5 · Validation — bad inputs"
# ─────────────────────────────────────────────────────────────────────────

# Missing long_url
R=$(post "/" '{"short_code":"hi"}')
assert_contains "Missing long_url → 422"     "$R" 'long_url'

# No http/https prefix
R=$(post "/" '{"long_url":"ftp://bad.com"}')
assert_contains "Invalid URL scheme → 422"   "$R" 'http'

# Invalid short_code chars
R=$(post "/" '{"long_url":"https://ok.com","short_code":"bad code!"}')
assert_contains "Bad short_code chars → 422" "$R" 'alphanumeric'

# long_url too long (>2048 chars)
LONG_URL="https://$(python3 -c "print('a'*2100)")"
R=$(post "/" "{\"long_url\":\"$LONG_URL\"}")
assert_contains "URL too long → 422"         "$R" 'error'

# Empty body
R=$(post "/" '{}')
assert_contains "Empty body → 422"           "$R" 'long_url'

# ─────────────────────────────────────────────────────────────────────────
header "6 · Redirect  (GET /:code)"
# ─────────────────────────────────────────────────────────────────────────

GITHUB_CODE="${CODE_MAP["https://github.com"]}"
R=$(get "/$GITHUB_CODE")
assert_contains "GET /$GITHUB_CODE returns location" "$R" 'github.com'
assert_contains "GET /$GITHUB_CODE returns 302 body" "$R" 'Redirecting'

# Non-existent code
R=$(get "/zzzzzzz")
assert_contains "Unknown code → 404" "$R" 'not found'

# ─────────────────────────────────────────────────────────────────────────
header "7 · Stats  (GET /:code/stats)"
# ─────────────────────────────────────────────────────────────────────────

# Hit the URL a few times first
for i in 1 2 3; do get "/$GITHUB_CODE" > /dev/null; done

R=$(get "/$GITHUB_CODE/stats")
assert_contains "Stats returns short_code"    "$R" '"short_code"'
assert_contains "Stats returns long_url"      "$R" 'github.com'
assert_contains "Stats returns hits"          "$R" '"hits"'
assert_contains "Stats returns created_at"    "$R" '"created_at"'
assert_contains "Stats returns last_accessed" "$R" '"last_accessed"'

HITS=$(echo "$R" | grep -o '"hits":[0-9]*' | sed 's/.*://')
if [ "$HITS" -ge 3 ] 2>/dev/null; then
    ok "Hit counter incremented correctly (hits=$HITS)"
else
    fail "Hit counter wrong (hits=$HITS)"
fi

# Stats for non-existent code
R=$(get "/zzzzzzz/stats")
assert_contains "Stats for unknown code → 404" "$R" 'not found'

# ─────────────────────────────────────────────────────────────────────────
header "8 · List  (GET /list)"
# ─────────────────────────────────────────────────────────────────────────

R=$(get /list)
assert_contains "GET /list returns total"  "$R" '"total"'
assert_contains "GET /list returns urls"   "$R" '"urls"'
assert_contains "GET /list returns offset" "$R" '"offset"'
assert_contains "GET /list returns limit"  "$R" '"limit"'

TOTAL=$(echo "$R" | grep -o '"total":[0-9]*' | sed 's/.*://')
if [ "$TOTAL" -ge 10 ] 2>/dev/null; then
    ok "List total=$TOTAL (at least 10 URLs created)"
else
    fail "List total=$TOTAL — expected at least 10"
fi

# Pagination
R=$(get "/list?limit=3&offset=0")
assert_contains "Paginated list (limit=3) has urls" "$R" '"short_code"'
COUNT=$(echo "$R" | grep -o '"short_code"' | wc -l | tr -d ' ')
if [ "$COUNT" -le 3 ]; then
    ok "Pagination limit respected (got $COUNT entries)"
else
    fail "Pagination limit NOT respected (got $COUNT entries)"
fi

# Offset past end
R=$(get "/list?limit=10&offset=9999")
assert_contains "Offset past end returns empty array" "$R" '"urls":\[\]'

# ─────────────────────────────────────────────────────────────────────────
header "9 · Delete  (DELETE /:code)"
# ─────────────────────────────────────────────────────────────────────────

# Create one to delete
R=$(post "/" '{"long_url":"https://delete-me.com"}')
DEL_CODE=$(echo "$R" | grep -o '"short_code":"[^"]*"' | sed 's/.*://;s/"//g')

STATUS=$(del_status "/$DEL_CODE")
assert_status "DELETE /$DEL_CODE returns 204" "$STATUS" "204"

# Confirm it's gone
R=$(get "/$DEL_CODE")
assert_contains "Deleted code returns 404 on GET"   "$R" 'not found'

R=$(get "/$DEL_CODE/stats")
assert_contains "Deleted code returns 404 on stats" "$R" 'not found'

# Delete non-existent
STATUS=$(del_status "/doesnotexist")
assert_status "DELETE non-existent returns 404" "$STATUS" "404"

# ─────────────────────────────────────────────────────────────────────────
header "10 · Custom code end-to-end"
# ─────────────────────────────────────────────────────────────────────────

R=$(post "/" '{"long_url":"https://anthropic.com/research","short_code":"aicorp"}')
assert_contains "Custom code created"       "$R" '"short_code":"aicorp"'

R=$(get "/aicorp")
assert_contains "Custom code resolves"      "$R" 'anthropic.com'

R=$(get "/aicorp/stats")
assert_contains "Custom code stats work"    "$R" '"short_code":"aicorp"'

STATUS=$(del_status "/aicorp")
assert_status "Custom code deleted"         "$STATUS" "204"

R=$(get "/aicorp")
assert_contains "Deleted custom code gone"  "$R" 'not found'

# ─────────────────────────────────────────────────────────────────────────
header "Results"
# ─────────────────────────────────────────────────────────────────────────
TOTAL_TESTS=$((PASS + FAIL))
echo ""
echo -e "  Passed : ${GREEN}${BOLD}$PASS${RESET} / $TOTAL_TESTS"
if [ "$FAIL" -gt 0 ]; then
    echo -e "  Failed : ${RED}${BOLD}$FAIL${RESET} / $TOTAL_TESTS"
    echo ""
    exit 1
else
    echo -e "\n  ${GREEN}${BOLD}All tests passed!${RESET}"
    echo ""
fi