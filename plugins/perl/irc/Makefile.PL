use ExtUtils::MakeMaker;

WriteMakefile('NAME' => 'Ekg2::Irc',
              'LIBS' => '',
	      'OBJECT' => '$(O_FILES)',
              'TYPEMAPS' => ['../common/typemap'],
              'INC' => '-I../../.. -I../../.. -I../../irc ',
	      'VERSION_FROM' => './Irc.pm');
