
	if (!strcasecmp(name, "private")) {
		int tmp;

		if (!params[0]) {
			printq((GG_S_F(config_status) ? "private_mode_is_on" : "private_mode_is_off"));
			return 0;
		}
		
		if ((tmp = on_off(params[0])) == -1) {
			printq("private_mode_invalid");
			return -1;
		}

		if (tmp == GG_S_F(config_status)) {
			printq(GG_S_F(config_status) ? "private_mode_is_on" : "private_mode_is_off");
			return 0;
		}

		printq(((tmp) ? "private_mode_on" : "private_mode_off"));
		ui_event("my_status", "private", ((tmp) ? "on" : "off"), NULL);

		config_status = GG_S(config_status);
		config_status |= ((tmp) ? GG_STATUS_FRIENDS_MASK : 0);

		if (sess && sess->state == GG_STATE_CONNECTED) {
			gg_debug(GG_DEBUG_MISC, "-- config_status = 0x%.2x\n", config_status);

			if (config_reason) {
				iso_to_cp(config_reason);
				gg_change_status_descr(sess, config_status, config_reason);
				cp_to_iso(config_reason);
			} else
				gg_change_status(sess, config_status);
		}
	}


