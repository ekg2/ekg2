/*
 *  (C) Copyright 2006+ Michal 'peres' Gorny
 *  			Michal 'GiM' Spadlinski <gim at skrzynka dot pl>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License Version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

function zwinRoster() {
	var left = xajax.$('left');
	var right = xajax.$('right');
	var zwijarka = xajax.$('zwijarka');
	if (right.style.display == 'block') {
		right.style.display = 'none';
		zwijarka.childNodes[0].nodeValue = '\u00ab';
		left.style.width = '99%';
	} else {
		right.style.display = 'block';
		zwijarka.childNodes[0].nodeValue = '\u00bb';
		left.style.width = '80%';
	}
}

window.onload = function() {
	/* zwijanie rostera */
	var roster = xajax.$('left');
	if (roster) {
		var zwijarka = document.createElement('a');
		zwijarka.href = 'javascript:zwinRoster();';
		zwijarka.id = 'zwijarka';
		var tekst = document.createTextNode('\u00bb');
		zwijarka.appendChild(tekst);
		roster.insertBefore(zwijarka, roster.firstChild);
	}
}

function update_windows_list()
{
	var el = xajax.$('windows_list');
	if (el)
	{
		el.innerHTML='';
		for (i=0; i<gwins.length; i++)
		{
			if (gwins[i] != undefined)
			{
//				alert('okienko:'+gwins[i][1]);
				ch = document.createElement('li');
				ch.setAttribute('id', 'wi'+i);

				aa = document.createElement('a');
				aa.setAttribute('id', 'awi'+i);
				aa.href = '#'; 
				aa.onclick = function() {
					// XXX WARNING DO NOT CHANGE THIS SILLY CODE!!!
					// WE CANNOT USE i INSTEAD!
					locz=this.id.substr(3);
					xajax.$('wi'+current_window).className='';
					xajax.$('wi'+locz).className='cur';
					current_window=locz;
					update_window_content(locz);
					return false;
				}
				aa.innerHTML=gwins[i][1];

				ch.appendChild(aa);
				if (gwins[i][0] == 2)
					ch.className="cur";
				else if (gwins[i][0] == 1)
					ch.className="act";
				el.appendChild(ch);
			}
		}
	}
}

function window_content_add_line(win)
{
	var el = xajax.$('window_content');
	if (el)
	{
		ch = document.createElement('li');
		ch.setAttribute('id', 'lin'+i);
		i=gwins[win][2].length-1;
		if (gwins[win][2][i].length > 0)
			ch.innerHTML=gwins[win][2][i];
		else
			ch.innerHTML="&nbsp;";
		if (i % 2)
			ch.className="info1";
		else
			ch.className="info2";
		el.appendChild(ch);
	}
}

function update_window_content(win)
{
	var el = xajax.$('window_content');
	if (el)
	{
		el.innerHTML='';
		for (i=0; i<gwins[win][2].length; i++)
		{
			if (gwins[win][2][i] != undefined)
			{
				ch = document.createElement('li');
				ch.setAttribute('id', 'lin'+i);
				if (gwins[win][2][i].length > 0)
					ch.innerHTML=gwins[win][2][i];
				else
					ch.innerHTML="&nbsp;";
				if (i % 2)
					ch.className="info1";
				else
					ch.className="info2";
				el.appendChild(ch);
			}
		}
	}

}
