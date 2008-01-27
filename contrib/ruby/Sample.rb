if $0 != "ekg2"
	print <<MSG
	warning: you are executing an embedded ruby file!
	this file is suppose to be run only from ekg2.
MSG
	exit
end

class Ekg2::Script::Sample < Ekg2::Script
	def theme_init
		format_add("dekoral", "%) %MCzas plynie a %YDEKORAL%n %gwciaz%n %TBIALY%n %B%1 uderzenie. %RZostalo %2. %GLosowa liczba: %3");
	end

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
		@already = @already + 1;
		@left = @left - 1;
		print "dekoral", @already.to_s, @left.to_s, (@already*rand(@left)).to_s
	end

	def varchange(name, newval)
		print "generic", "Zmienna " + name + " zmienila wartosc na " + newval;
	end
    
	def initialize
		super

		command_bind("foo", "handler_foo")
#		handler_bind("ui-keypress", "handler_keypress")
		timer_bind(1, "handler_czasomierz");
		variable_add("zmienna_testowa", "wartosc", "varchange")

		@already = 0
		@left = 1000000;
	end

	def finalize
		print "Sprzatam!"
	end

end

