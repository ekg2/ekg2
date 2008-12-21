if $0 != "ekg2"
	print <<MSG
	warning: you are executing an embedded ruby file!
	this file is suppose to be run only from ekg2.
MSG
	exit
end

class Ekg2::Script::WatchSample < Ekg2::Script
	def fd2IO(fd)
		return IO.for_fd(fd, 'r')
	end

	def handler_foo_watch(type, fd, watch)
		return 0 if type != 0;

		rd = fd2IO(fd)

		print "generic", "fd: " + fd.to_s + "; Odebrano: " + rd.read();
		rd.close();
	end

	def handler_foo(parametr = nil)
		rd, wr = IO.pipe

		if fork
			wr.close
			watch_add(rd.fileno, WATCH_READ, "handler_foo_watch");
		else
			rd.close
			wr.write "test"
			sleep 1
			wr.close
			Process.exit!(0);
		end
	end

	def initialize
		super

		command_bind("foo", "handler_foo")
	end

end
