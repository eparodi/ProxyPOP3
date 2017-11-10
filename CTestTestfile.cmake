# CMake generated Testfile for 
# Source directory: /home/flacu/Downloads/MIMEstrip/ProxyPOP3
# Build directory: /home/flacu/Downloads/MIMEstrip/ProxyPOP3
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(check_parser "/home/flacu/Downloads/MIMEstrip/ProxyPOP3/tests/check_parser")
add_test(check_parser_utils "/home/flacu/Downloads/MIMEstrip/ProxyPOP3/tests/check_parser_utils")
add_test(check_mime_msg "/home/flacu/Downloads/MIMEstrip/ProxyPOP3/tests/check_mime_msg")
subdirs(src)
subdirs(tests)
