if $0 != "ekg2"
	print <<MSG
	warning: you are executing an embedded ruby file!
	this file is suppose to be run only from ekg2.
MSG
	exit
end

class Ekg2::Script::Sample < Ekg2::Script
	def handler_keypress(klawisz)
		print "Nacisnales klawisz!: #" + klawisz.to_s;
	end

	def initialize
		super
		handler_bind("ui-keypress", "handler_keypress")
	end

	def foo
		print "super"
	end
end
