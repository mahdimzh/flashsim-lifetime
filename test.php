<?php
ini_set('memory_limit', '-1');
//$fileData = file_get_contents('/Users/Mahdi/Desktop/flashsim-master/output.csv');
$fileData1 = file_get_contents('/Users/Mahdi/Desktop/flashsim-master/output1.csv');

//$data = explode("\n",$fileData);
$data1 = explode("\n",$fileData1);
$vals = array_count_values($data1);

//$myfile = fopen("/Users/Mahdi/Desktop/flashsim-master/output.csv", "w");
$myfile1 = fopen("/Users/Mahdi/Desktop/flashsim-master/output1.csv", "w");
//echo count($data). ";" . (count($data)*4) ."\n";
echo "\n" . count($data1) . "\n";

/*foreach ($data as $key => $value) {
$newValue =  $key*4 . ";" .($value). "\n";
	fwrite($myfile, $newValue);
	//echo $key;
}*/

foreach ($vals as $key => $value) {
	if($value != "")
		fwrite($myfile1, $key . ";" . $value . "\n");
}
