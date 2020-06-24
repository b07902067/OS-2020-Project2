### About Sample Input
- sample_input_1 and sample_input_2 are the sample input provided by TAs.

### About Test Input
- test_input_1 and test_input_2 are the input which we design.
	- Having one large file with 406000 file size in test_input_1 and multiple files with total 40600 file size in sample_input_2.
- Both test_data_set are smaller than a MMAP size(409600), but the data set with multiple small files need 10 times of mmap, the data set with one large file only need one time. So we predict that the performance of test_input_2 will worse than test_input_1 with mmap.
- Result stated in report.
