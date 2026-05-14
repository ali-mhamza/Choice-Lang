import os
import subprocess
import sys
from pathlib import Path
from subprocess import PIPE

# Constants

PROMPT = ">>> "
EXE_FILE = "choice-release"
EXE_DIR = "."
TEST_DIR = "test"

# Put this before the output you expect to receive.
EXPECT_SIGN = "// Expect: "
# Put this to indicate that you expect some number, without specifying.
# E.g., // Expect: [num]
EXPECT_NUM = "[num]"
# Put this to have the test framework interpret the expect text
# with escape sequence encoding, or as pure byte sequences.
# E.g., // Expect: [bytes] ab\r\n
EXPECT_BYTES = "[bytes] "

# Any file testing errors must end its filename with this (not
# counting the extension).
ERROR_FILE_MARK = "_error"
# General way to indicate that this input leads to an error
ERROR_SIGN = "// Error"
# The way to indicate that a certain number of errors is expected.
# E.g., // Error(2) = 2 errors expected.
ERROR_NUM_SIGN = "// Error ("

# Put this at the top of a file you wish to not test.
IGNORE_SIGN = "# Ignore #"

EXPECT_LEN = len(EXPECT_SIGN)
EXPECT_BYTES_LEN = len(EXPECT_BYTES)
ERROR_LEN = len(ERROR_SIGN)
ERROR_NUM_LEN = len(ERROR_NUM_SIGN)

# Number of fails it will stop at.
MAX_FAIL = 10
# Flag to abort the tests if MAX_FAIL tests failed.
ABORT_MAX_FAIL = False

# Will vertically align the test results.
PAD_TEST_STATUS = False

# Colors

NORMAL = "\033[39m"
RED = "\033[31m"
GREEN = "\033[32m"
YELLOW = "\033[33m"
CYAN = "\033[36m"


def start_checks(chosen_dir: str | None) -> None:
    if not os.path.exists(f"{EXE_DIR}/{EXE_FILE}"):
        print(f"{RED}No test executable found.{NORMAL}", file=sys.stderr)
        exit()
    if (chosen_dir is not None) and not os.path.exists(chosen_dir):
        print(f"{RED}No such test directory.{NORMAL}")
        exit()


def set_up(chosen_dir: str | None) -> dict[str, list[str]]:
    tests: dict[str, list[str]] = {}
    if chosen_dir is None:
        path = Path(f"{TEST_DIR}")
        for test_dir in path.iterdir():
            if test_dir.is_dir():
                tests[test_dir.name] = {}
    else:
        tests[chosen_dir[len(TEST_DIR) + 1 :]] = {}

    for dir_name in tests:
        path = Path(f"{TEST_DIR}/{dir_name}")
        test_paths: list[str] = []
        for test in path.iterdir():
            test_paths.append(f"{TEST_DIR}/{dir_name}/{test.name.rstrip()}")
        tests[dir_name] = test_paths
    return tests


def parse_script(path: str) -> list[str] | None:
    expects: list[str] = []
    with open(path) as file:
        lines = file.readlines()
        lines = [line.rstrip() for line in lines]
        if len(lines) > 0 and lines[0] == IGNORE_SIGN:
            return None

        for line in lines:
            pos = line.find(EXPECT_SIGN)
            if pos != -1:
                insert = line[pos + EXPECT_LEN :]
                new_pos = insert.find(EXPECT_BYTES)
                if new_pos != -1:
                    expects.append(
                        insert[new_pos + EXPECT_BYTES_LEN :]
                        .encode("utf-8")
                        .decode("unicode_escape")
                    )
                else:
                    expects.append(line[pos + EXPECT_LEN :])
    return expects


def parse_error_script(path: str) -> tuple[list[str], int] | None:
    lines: list[str] = []
    errors = 0
    with open(path) as file:
        lines = file.readlines()
        lines = [line.rstrip() for line in lines]
        # Remove empty lines since they will end the
        # REPL session we use for testing.
        lines = [line for line in lines if line != ""]
        if len(lines) > 0 and lines[0] == IGNORE_SIGN:
            return None

        for line in lines:
            if (pos := line.find(ERROR_NUM_SIGN)) != -1:
                num = int(line[pos + ERROR_NUM_LEN : -1])
                errors += num
            elif ERROR_SIGN in line:
                errors += 1
    return (lines, errors)


def run_script(path: str) -> list[str]:
    proc = subprocess.run(
        # Turn off stdout buffering for test.
        ["stdbuf", "-oL", f"{EXE_DIR}/{EXE_FILE}", path],
        stdout=PIPE,
    )

    lines = proc.stdout.splitlines()
    for i, line in enumerate(lines):
        try:
            lines[i] = line.decode("utf-8")
        except:
            # Output is in bytes.
            lines[i] = line.decode("unicode_escape")
    return lines


def run_error_script(lines: list[str]) -> int:
    input_text = "\n".join(lines) + "\n"

    proc = subprocess.run(
        f"{EXE_DIR}/{EXE_FILE}",
        input=input_text,
        text=True,
        stdout=PIPE,  # To discard output.
        stderr=PIPE,
    )

    error_lines = proc.stderr.splitlines()
    errors = len(error_lines)
    for error in error_lines:
        if "E00" not in error:
            errors -= 1

    return errors


def compare_result_expected(result: list[str], expect: list[str]) -> bool:
    if len(result) != len(expect):
        return False
    for r, e in zip(result, expect):
        if (e == EXPECT_NUM) and (r.isdecimal()):
            continue
        # Hard coding this particular case.
        # '\n' and ''.
        elif e.rstrip("\n") == r.rstrip("\n"):
            continue
        elif r != e:
            return False
    return True


def run_regular_test(path: str) -> tuple[bool, list[str], list[str]] | None:
    expect = parse_script(path)
    if expect is None:
        return None
    result = run_script(path)
    return (compare_result_expected(result, expect), expect, result)


def run_error_test(path: str) -> tuple[bool, int, int]:
    expect = parse_error_script(path)
    if expect is None:
        return None
    result = run_error_script(expect[0])
    return (result == expect[1], expect[1], result)


def print_test_title(test: str, error_test: bool, print_output: bool) -> None:
    if not print_output:
        return

    if error_test:
        print(f"{CYAN}{test}:{NORMAL}", end=" ")
    else:
        print(f"{test}:", end=" ")


def handle_test_outcome(
    outcome,
    score: list[int, int, int],
    test: str,
    error_test: bool,
    print_output: bool,
    max_test_length: int,
) -> list[int, int, int]:
    print_test_title(test, error_test, print_output)
    padding = max_test_length - len(f"{test}") if PAD_TEST_STATUS else 0

    if outcome is None:
        score[2] += 1
        if print_output:
            print(" " * padding + f"{YELLOW}ignored{NORMAL}")
            return score

    if outcome[0]:
        score[0] += 1
    else:
        score[1] += 1
        if ABORT_MAX_FAIL and score[1] == MAX_FAIL:
            print(f"{RED}MAXIMUM FAILS REACHED.")
            print(f"ABORTING...{NORMAL}")
            return score

    if print_output:
        if outcome[0]:
            print(" " * padding + f"{GREEN}passed{NORMAL}")
        else:
            print(" " * padding + f"{RED}failed{NORMAL}")
            if error_test:
                print(f"    {RED}Expected errors:{NORMAL} {outcome[1]}")
                print(f"    {RED}Received errors:{NORMAL} {outcome[2]}")
            else:
                for i, (expect, result) in enumerate(zip(outcome[1], outcome[2])):
                    if (expect == EXPECT_NUM) and (result.isdecimal()):
                        continue
                    elif expect != result:
                        print(f"    {i + 1}: {RED}Expected:{NORMAL} {expect}")
                        print(f"    {i + 1}: {RED}Received:{NORMAL} {result}")
                diff = len(outcome[1]) - len(outcome[2])
                if diff > 0:
                    print(f"    {RED}Expected exceeded output by {diff}{NORMAL}")
                elif diff < 0:
                    print(f"    {RED}Output exceeded expected by {-diff}{NORMAL}")

    return score


def run_tests(
    tests: dict[str, list[str]], print_output: bool, max_test_length: int
) -> None:
    passed, failed, ignored = 0, 0, 0

    for dir, dir_tests in tests.items():
        if print_output:
            print(f"=== {dir} ===")
        for test in dir_tests:
            outcome = None
            error_test = False

            if ERROR_FILE_MARK in test:
                outcome = run_error_test(test)
                error_test = True
            else:
                outcome = run_regular_test(test)

            passed, failed, ignored = handle_test_outcome(
                outcome,
                [passed, failed, ignored],
                test,
                error_test,
                print_output,
                max_test_length,
            )
        if print_output:
            print()

    print("FINAL STATUS:", end=" ")
    if failed == 0:
        print(f"{GREEN}PASS{NORMAL}")
    else:
        print(f"{RED}FAIL{NORMAL}")
    print(f"TOTAL: {passed + failed + ignored}")
    if passed != 0:
        print(f"{GREEN}PASSED: {passed}{NORMAL}")
    if failed != 0:
        print(f"{RED}FAILED: {failed}{NORMAL}")
    if ignored != 0:
        print(f"{YELLOW}IGNORED: {ignored}{NORMAL}")


def get_max_test_length(tests: dict[str, list[str]]) -> int:
    max_length = 0
    for collection in tests.values():
        length = len(max(collection, key=lambda l: len(l)))
        if max_length < length:
            max_length = length
    return max_length


if __name__ == "__main__":
    print_output = True
    dir: str | None = None
    if "--quiet" in sys.argv:
        if len(sys.argv) > 2:
            dir = sys.argv[-1]
        print_output = False
    else:
        if len(sys.argv) > 1:
            dir = sys.argv[-1]

    start_checks(dir)
    tests = set_up(dir)
    max_test_length = get_max_test_length(tests)
    if dir is not None:
        num_tests = len(tests[dir[len(TEST_DIR) + 1 :]])
        print(f"Running {num_tests} tests...\n")
    run_tests(tests, print_output, max_test_length)