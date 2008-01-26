if $0 != "ekg2"
	print <<MSG
	warning: you are executing an embedded ruby file!
	this file is suppose to be run only from ekg2.
MSG
	exit
end

class Ekg2::Script::Sample < Ekg2::Script
	def handler_foo(parametr = nil)
		if parametr != nil
			print "Wywolane polecenie foo z parametrem: `" + parametr + "` !"
		else
			print "Wywolane polecenie foo bez parametrow!"
		end
	end

	def handler_keypress(klawisz)
		print "Nacisnales klawisz!: #" + klawisz.to_s
	end

	def handler_czasomierz()
		print "Czas plynie a dekoral wciaz bialy"
	end

	def initialize
		super
#		handler_bind("ui-keypress", "handler_keypress")
		command_bind("foo", "handler_foo")
		timer_bind(1, "handler_czasomierz");
	end

end
