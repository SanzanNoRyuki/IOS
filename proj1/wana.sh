#!/bin/sh

POSIXLY_CORRECT=yes

while [ "$#" != "0" ]																																		#Nacitanie argumentov
do
	case "$1" in
		"-a")
			if [ "$after_date" != "" ]
				then
					printf "incorrect input - multiple after date filters\n" >&2
					exit 4
				else
					after_date=$2

					if ! (echo "$after_date" | grep -qE '^([0-9]{4}\-(0[1-9]|1[0-2])\-(0[1-9]|[1-2][0-9]|3[0-1]) ([0-1][0-9]|2[0-3])(\:[0-5][0-9]){2})$')	#YYYY-MM-DD HH:MM:SS
						then
							printf "incorrect input - incorrect date format\n" >&2
							exit 3
						else
							shift 1
					fi
			fi
			;;
		"-b")
			if [ "$before_date" != "" ]
				then
					printf "incorrect input - multiple before date filters\n" >&2
					exit 4
				else
					before_date=$2

					if ! (echo "$before_date" | grep -qE '^([0-9]{4}\-(0[1-9]|1[0-2])\-(0[1-9]|[1-2][0-9]|3[0-1]) ([0-1][0-9]|2[0-3])(\:[0-5][0-9]){2})$')	#YYYY-MM-DD HH:MM:SS
						then
							printf "incorrect input - incorrect date format\n" >&2
							exit 3
						else
							shift 1
					fi
			fi
			;;
		"-ip")
			if [ "$ip_adress" != "" ]
				then
					printf "incorrect input - multiple IP adress filters\n" >&2
					exit 4
				else
					ip_adress=$2

					if ! (echo "$ip_adress" | grep -qE '^((([0-9]{1,3}\.){3}[0-9]{1,3})|(([0-9A-Fa-f]{0,4}\:){3,7}[0-9A-Fa-f]{0,4}))$')						#IPv4 || IPv6
						then
							printf "incorrect input - incorrect IP format\n" >&2
							exit 3
						else
							shift 1
					fi
			fi
			;;
		"-uri")
			if [ "$uri" != "" ]
				then
					printf "incorrect input - multiple URI filters\n" >&2
					exit 4
				else
					uri=$2

					if ! (echo "$uri" | grep -qE '/.*')																										#/.*
						then
							printf "incorrect input - incorrect URI format\n" >&2
							exit 3
						else
							shift 1
					fi
			fi
			;;

		"list-ip")
			if [ "$instruction" != "" ]
				then
					printf "incorrect input - only one instruction is expected\n" >&2
					exit 2
				else
					instruction="list-ip"
			fi
			;;
		"list-hosts")
			if [ "$instruction" != "" ]
				then
					printf "incorrect input - only one instruction is expected\n" >&2
					exit 2
				else
					instruction="list-hosts"
			fi
			;;
		"list-uri")
			if [ "$instruction" != "" ]
				then
					printf "incorrect input - only one instruction is expected\n" >&2
					exit 2
				else
					instruction="list-uri"
			fi
			;;
		"hist-ip")
			if [ "$instruction" != "" ]
				then
					printf "incorrect input - only one instruction is expected\n" >&2
					exit 2
				else
					instruction="hist-ip"
			fi
			;;
		"hist-load")
			if [ "$instruction" != "" ]
				then
 					printf "incorrect input - only one instruction is expected\n" >&2
 					exit 2
				else
					instruction="hist-load"
			fi
			;;

		*)
			logfiles="$logfiles $1"
			;;
	esac

	shift 1																																					#Posun argumentu
done


if [ "$logfiles" = "" ]																																		#Nacitanie zaznamov
	then
		log="$(cat)"
	else
		newline="
"
		for logfile in $logfiles
		do
			if [ -f "$logfile" ]
				then
					if (echo "$logfile" | grep -qe '\.gz$')
						then
							log="$log$(gunzip -dfc "$logfile")$newline"																						#Komprimovany zaznam
						else
							log="$log$(cat "$logfile")$newline"																								#Nekomprimovany zaznam
					fi
				else
					printf "incorrect input - \"%s\" couldn't be openned\n" "$logfile" >&2
					exit 1
			fi
		done
fi

if [ "$after_date" != "" ]																																	#Filtrovanie zaznamov
	then																																					#v~ YYYYMMDDHHMMSS
		after_date="$(printf "%s" "$after_date" | awk '{gsub ("-", "");
														gsub (":", "");
														gsub (" ", "");
														print $0}')"

		log="$(printf "%s" "$log" | awk -v test_date="$after_date" '{	logline=$0
																		$0=	substr($4, 2, length($4)-1);
																			gsub ("/", " ", $0);
																			gsub (":", " ", $0);
																			gsub ("Jan", "01", $2);
																			gsub ("Feb", "02", $2);
																			gsub ("Mar", "03", $2);
																			gsub ("Apr", "04", $2);
																			gsub ("May", "05", $2);
																			gsub ("Jun", "06", $2);
																			gsub ("Jul", "07", $2);
																			gsub ("Aug", "08", $2);
																			gsub ("Sep", "09", $2);
																			gsub ("Oct", "10", $2);
																			gsub ("Nov", "11", $2);
																			gsub ("Dec", "12", $2);
																			date= $3$2$1$4$5$6
																			if (date>test_date) print logline}')"
fi

if [ "$before_date" != "" ]
	then																																					#v~ YYYYMMDDHHMMSS
		before_date="$(printf "%s" "$before_date" | awk '{	gsub ("-", "");
															gsub (":", "");
															gsub (" ", "");
															print $0}')"

		log="$(printf "%s" "$log" | awk -v test_date="$before_date" '{	logline=$0
																		$0=	substr($4, 2, length($4)-1);
																			gsub ("/", " ", $0);
																			gsub (":", " ", $0);
																			gsub ("Jan", "01", $2);
																			gsub ("Feb", "02", $2);
																			gsub ("Mar", "03", $2);
																			gsub ("Apr", "04", $2);
																			gsub ("May", "05", $2);
																			gsub ("Jun", "06", $2);
																			gsub ("Jul", "07", $2);
																			gsub ("Aug", "08", $2);
																			gsub ("Sep", "09", $2);
																			gsub ("Oct", "10", $2);
																			gsub ("Nov", "11", $2);
																			gsub ("Dec", "12", $2);
																			date= $3$2$1$4$5$6
																			if (date<test_date) print logline}')"
fi

if [ "$ip_adress" != "" ]
	then
		log="$(printf "%s" "$log" | awk -v test_ip="$ip_adress" '{if ($1==test_ip) print $0}')"
fi

if [ "$uri" != "" ]
	then
		log="$(printf "%s" "$log" | awk -v test_uri="$uri" '{if ($7==test_uri) print $0}')"
fi


case "$instruction" in																																		#Plnenie prikazov
	"")
		printf "%s" "$log" | sort -u
		;;
	"list-ip")
		printf "%s" "$log" | awk '{print $1}' | sort -u
		;;
	"list-hosts")
		iplist="$(printf "%s" "$log" | awk '{print $1}' | sort -u)"

		for ip in $iplist
		do
			host "$ip" | awk -v ip="$ip" '{ if ($1=="Host") print ip;
											else if ($2=="connection");
											else print $NF}'																								#^~ "Conection timed out" - nič nevypíše
		done
		;;
	"list-uri")
		printf "%s" "$log" | awk '{if ($6=="\"GET" || $6=="\"HEAD" || $6=="\"POST") print $7}' | sort -u
		;;
	"hist-ip")
		printf "%s" "$log" | awk '{print $1}' | sort | uniq -c | sort -nr | awk '{	printf "%s (%i): ", $2, $1;
																					for (i=0;i<$1;i++) printf "#";
																					printf "\n"}'
		;;
	"hist-load")
		printf "%s" "$log" | awk '{ $4=substr($4, 2, length($4)-1);
									gsub ("/", " ", $4);
									gsub (":", " ", $4);
									print $4}' | awk '{	gsub ("Jan", "01", $2);
														gsub ("Feb", "02", $2);
														gsub ("Mar", "03", $2);
														gsub ("Apr", "04", $2);
														gsub ("May", "05", $2);
														gsub ("Jun", "06", $2);
														gsub ("Jul", "07", $2);
														gsub ("Aug", "08", $2);
														gsub ("Sep", "09", $2);
														gsub ("Oct", "10", $2);
														gsub ("Nov", "11", $2);
														gsub ("Dec", "12", $2);
														printf "%s-%s-%s %s:00\n", $3, $2, $1, $4}' |
															sort | uniq -c | awk '{	printf "%s %s (%i): ", $2, $3, $1;
																					for (i=0;i<$1;i++) printf "#";
																					printf "\n"}' | sort
		;;
esac
