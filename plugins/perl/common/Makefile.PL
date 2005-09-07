use ExtUtils::MakeMaker;

WriteMakefile('NAME' => 'Ekg2',
              'LIBS' => '',
	      'OBJECT' => '$(O_FILES)',
              'INC' => '-I../../.. -I../../../libltdl ',
	      'VERSION_FROM' => './Ekg2.pm');
