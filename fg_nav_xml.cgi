#!/usr/bin/perl -Tw
#
#
# Author: Pigeon <pigeon at pigeond dot net>
#
# Under debian, you need libdbix-easy-perl,
# libdbd-pg-perl (postgres) or libdbd-mysql-perl (mysql)
#

use strict;
use DBIx::Easy;


my($APT_TABLE) = "fg_apt";
my($APTWAY_TABLE) = "fg_apt_way";
my($ATC_TABLE) = "fg_atc";
my($NAV_TABLE) = "fg_nav";
my($FIX_TABLE) = "fg_fix";
my($AWY_TABLE) = "fg_awy";

my($SQL_LIMIT) = 100;


print("Pragma: no-cache\r\n");
print("Cache-Control: no-cache\r\n");
print("Expires: Sat, 17 Sep 1977 00:00:00 GMT\r\n");
print("Content-Type: text/xml\r\n\r\n");


sub err_print
{
    my($err) = @_;
    print("<navaids err=\"${err}\" />\n");
    exit(0);
}


sub min
{
    ($a, $b) = @_;
    return ($a < $b ? $a : $b);
}

sub htmlencode
{
    my($str) = @_;
    $str =~ s/\&/\&amp;/g;
    $str =~ s/\</\&lt;/g;
    $str =~ s/\>/\&gt;/g;
    return $str;
}

sub rwy_nav
{
    my($dbi, $apt_code, $rwy_num) = @_;
    my($rwy_nav) = "";
    my($k);

    my($nav_sql) = "SELECT * FROM ${NAV_TABLE}";
    $nav_sql .= " WHERE (nav_type=4 OR nav_type=5 OR ".
        "nav_type=6 OR nav_type=7 OR nav_type=8 OR ".
        "nav_type=9 OR nav_type=12)";
    $nav_sql .= " AND UPPER(name) LIKE '".
                    uc(${apt_code})." ".uc(${rwy_num})." %'";
    $nav_sql .= ";";

    my($nav_sth) = $dbi->process($nav_sql);

    for($k = 0; $k < $nav_sth->rows; $k++)
    {
        my($nav_href) = $nav_sth->fetchrow_hashref;
        my(%nav_hash) = %$nav_href;

        my($nav_name) = &htmlencode($nav_hash{'name'});
        $nav_name =~ s/${apt_code} ${rwy_num}//gi;

        my($nav_type) = $nav_hash{'nav_type'};
        my($nav_lat) = $nav_hash{'lat'};
        my($nav_lng) = $nav_hash{'lng'};
        my($nav_elevation) = $nav_hash{'elevation'};
        my($nav_freq) = &freqstr($nav_hash{'freq'});
        my($nav_range) = $nav_hash{'range'};
        my($nav_multi) = $nav_hash{'multi'};
        my($nav_ident) = $nav_hash{'ident'};

        my($nav_tag);
        $nav_tag = "ils" if($nav_type eq '4');
        $nav_tag = "ils" if($nav_type eq '5');
        $nav_tag = "gs" if($nav_type eq '6');
        $nav_tag = "om" if($nav_type eq '7');
        $nav_tag = "mm" if($nav_type eq '8');
        $nav_tag = "im" if($nav_type eq '9');
        $nav_tag = "ilsdme" if($nav_type eq '12');

        $rwy_nav .= <<XML;
\t\t\t\t<${nav_tag} name="${nav_name}" type="${nav_type}" lat="${nav_lat}" lng="${nav_lng}" elevation="${nav_elevation}" freq="${nav_freq}" range="${nav_range}" multi="${nav_multi}" ident="${nav_ident}" />
XML
    }

    return $rwy_nav;
}


sub reverse_rwy
{
    my($rwy_num) = @_;
    my($num, $lr) = ($rwy_num =~ /(\d+)(.*)/);

    if(!$num)
    {
        return ${rwy_num};
    }

    if($num > 18)
    {
        $num -= 18;
    }
    else
    {
        $num += 18;
    }

    if($lr ne "")
    {
        if(uc($lr) eq "L")
        {
            $lr = "R";
        }
        else
        {
            $lr = "L";
        }
    }

    return "${num}${lr}";
}

sub freqstr
{
    my($freq) = @_;
    return sprintf("%.2f", $freq / 100);
}

my($xml) = "";


if(!$ENV{'QUERY_STRING'})
{
    exit(0);
}


# search string
my($sstr);

# search options
my($apt_code, $apt_name, $vor, $ndb, $fix, $awy, $taxiway, $bounds);

# bounds
my($ne, $sw, $ne_lat, $ne_lng, $sw_lat, $sw_lng);

my($p);
my(@a) = split(/\&/, $ENV{'QUERY_STRING'});

foreach $p (@a)
{
    my($key, $value) = split(/=/, $p);

    if($key eq 'sstr')
    {
        $sstr = $value;
    }
    elsif($key eq 'apt_name')
    {
        $apt_name = 1;
    }
    elsif($key eq 'apt_code')
    {
        $apt_code = 1;
    }
    elsif($key eq 'vor')
    {
        $vor = 1;
    }
    elsif($key eq 'ndb')
    {
        $ndb = 1;
    }
    elsif($key eq 'fix')
    {
        $fix = 1;
    }
    elsif($key eq 'awy')
    {
        $awy = 1;
    }
    elsif($key eq 'taxiway')
    {
        # We want taxiway too
        $taxiway = 1;
    }
    elsif($key eq 'ne')
    {
        $ne = $value;
        ($ne_lat, $ne_lng) = split(/,/, $ne);
        if($ne_lng < 0)
        {
            $ne_lng += 360;
        }
    }
    elsif($key eq 'sw')
    {
        $sw = $value;
        ($sw_lat, $sw_lng) = split(/,/, $sw);
        if($sw_lng < 0)
        {
            $sw_lng += 360;
        }
    }
}

my($dbi);
my($sth);

#$dbi = new DBIx::Easy qw(mysql flightgear fgmap);
$dbi = new DBIx::Easy qw(Pg flightgear fgmap);

if(!$sstr and !($ne and $sw))
{
    &err_print("Invalid search");
}

if($sstr)
{
    $sstr =~ s/%([a-fA-F0-9][a-f-A-F0-9])/pack("C", hex($1))/ge;
    $sstr =~ s/[\%\*\.\?\_]//g;

    if(length($sstr) < 2)
    {
        &err_print("Search string too short, minimum length 3.");
    }
}

if($ne and $sw)
{
    # Check if the bounds is way too big, we want to limit this

    # this is gmap z=10
    my($lat_max) = 0.5199925335038184;
    my($lng_max) = 1.1714172363281472;

    if((($ne_lat - $sw_lat) > $lat_max) or
            (($ne_lng - $sw_lng) > $lng_max))
    {
        &err_print("Bounds too high");
    }
}


if(!$dbi)
{
    # TODO
    &err_print("Database connector error");
}


my($result_cnt) = 0;
my($sql);



if($apt_code or $apt_name)
{
    # airport
    $xml .= "\t<apt>\n";

    if($sstr)
    {
        $sql = "SELECT * FROM ${APT_TABLE} WHERE ";

        if($apt_code)
        {
            $sql .= "UPPER(apt_code) LIKE '\%".uc(${sstr})."\%'";
        }

        if($apt_name)
        {
            if($apt_code)
            {
                $sql .= " OR ";
            }
            $sql .= "UPPER(apt_name) LIKE '\%".uc(${sstr})."\%'";
        }

        $sql .= " ORDER BY apt_code;";
    }
    elsif($ne and $sw)
    {
        # Searching for airport according to its runway
        $sql = "SELECT DISTINCT ON (${APT_TABLE}.apt_id) * FROM ${APT_TABLE} ";
        $sql .= "JOIN ${APTWAY_TABLE} ON ";
        $sql .= "${APT_TABLE}.apt_id = ${APTWAY_TABLE}.apt_id";
        $sql .= " WHERE ${APTWAY_TABLE}.type = 'r'";
        $sql .= " AND (lat < ${ne_lat}";
        $sql .= " AND lat > ${sw_lat}";
        $sql .= " AND abslng < ${ne_lng}";
        $sql .= " AND abslng > ${sw_lng})";
        $sql .= " ORDER BY ${APT_TABLE}.apt_id, apt_code;";
    }


    #print(STDERR "$sql\n");
    $sth = $dbi->process($sql);

    if($sth->rows > 0)
    {
        my($i, $j);

        #for($i = 0; $i < $sth->rows; $i++)
        for($i = 0; $i < (&min(${SQL_LIMIT} - ${result_cnt}, $sth->rows)); $i++)
        {
            my($row_href) = $sth->fetchrow_hashref;
            my(%row_hash) = %$row_href;

            my($apt_id) = $row_hash{'apt_id'};
            my($apt_code) = $row_hash{'apt_code'};
            my($apt_name) = &htmlencode($row_hash{'apt_name'});
            my($elevation) = $row_hash{'elevation'};


            $xml .= <<XML;
\t\t<airport id="${apt_id}" code="${apt_code}" name="${apt_name}" elevation="${elevation}">
XML

            # Get runway/taxiway
            my($aptway_sql) =
                "SELECT * FROM ${APTWAY_TABLE} WHERE apt_id=${apt_id}";
            if(!$taxiway)
            {
                $aptway_sql .= " AND type = 'r'";
            }
            $aptway_sql .= " ORDER BY type;";

            my($apt_sth) = $dbi->process($aptway_sql);
            
            for($j = 0; $j < $apt_sth->rows; $j++)
            {
                # For each runway...
                my($aptway_href) = $apt_sth->fetchrow_hashref;
                my(%aptway_hash) = %$aptway_href;

                my($type) = $aptway_hash{'type'};
                my($num) = $aptway_hash{'num'};

                #$num =~ s/^\s*//g;
                #$num =~ s/\s*$//g;

                my($lat) = $aptway_hash{'lat'};
                my($lng) = $aptway_hash{'lng'};
                my($heading) = $aptway_hash{'heading'};
                my($length) = $aptway_hash{'length'};
                my($width) = $aptway_hash{'width'};

                if($type eq "r")
                {
                    $xml .= <<XML;
\t\t\t<runway num="${num}" lat="${lat}" lng="${lng}" heading="${heading}" length="${length}" width="${width}">
XML
                    $xml .= &rwy_nav($dbi, $apt_code, ${num});
                    $xml .= "\t\t\t</runway>\n\n";

                    # The reverse runway
                    my($rnum) = &reverse_rwy(${num});

                    if(${rnum} ne ${num})
                    {
                        if($heading > 180)
                        {
                            $heading -= 180;
                        }
                        else
                        {
                            $heading += 180;
                        }

                        $xml .= <<XML;
\t\t\t<runway num="${rnum}" lat="${lat}" lng="${lng}" heading="${heading}" length="${length}" width="${width}">
XML
                        $xml .= &rwy_nav($dbi, $apt_code, ${rnum});
                        $xml .= "\t\t\t</runway>\n\n";
                    }
                }
                elsif($type eq "t")
                {
                    $xml .= <<XML;
\t\t\t<taxiway lat="${lat}" lng="${lng}" heading="${heading}" length="${length}" width="${width}" />
XML
                }

            }

            # Get the ATC stuff
            my($atc_sql) =
                "SELECT * FROM ${ATC_TABLE} WHERE apt_id=${apt_id} ORDER BY atc_type;";
            my($atc_sth) = $dbi->process($atc_sql);
            
            for($j = 0; $j < $atc_sth->rows; $j++)
            {
                my($atc_href) = $atc_sth->fetchrow_hashref;
                my(%atc_hash) = %$atc_href;
                my($atc_type) = $atc_hash{'atc_type'};
                my($freq) = &freqstr($atc_hash{'freq'});
                my($name) = &htmlencode($atc_hash{'name'});

                $xml .= <<XML;
\t\t\t<atc atc_type="${atc_type}" freq="${freq}" name="${name}" />
XML
            }

            $xml .= "\t\t</airport>\n\n";
        }
        $result_cnt += $sth->rows;
    }

    $xml .= "\t</apt>\n\n";
}


if($vor or $ndb)
{
    $sql = "SELECT * FROM ${NAV_TABLE} WHERE ";

    if($sstr)
    {
        $sql .= "(UPPER(ident) LIKE '\%".uc(${sstr})."\%' OR ";
        $sql .= "UPPER(name) LIKE '\%".uc(${sstr})."\%')";
    }
    elsif($ne and $sw)
    {
        $sql .= "(lat < ${ne_lat}";
        $sql .= " AND lat > ${sw_lat}";
        $sql .= " AND abslng < ${ne_lng}";
        $sql .= " AND abslng > ${sw_lng})";
    }

    $sql .= " AND (";

    if($vor)
    {
        $sql .= "nav_type = 3";
    }

    if($ndb)
    {
        if($vor)
        {
            $sql .= " OR ";
        }

        $sql .= " nav_type = 2";
    }

    $sql .= ")";

    $sql .= " ORDER BY ident;";

    $sth = $dbi->process($sql);

    if($sth->rows > 0)
    {
        my($i);
        for($i = 0; $i < (&min(${SQL_LIMIT} - ${result_cnt}, $sth->rows)); $i++)
        {
            my($row_href) = $sth->fetchrow_hashref;
            my(%row_hash) = %$row_href;

            my($nav_type) = $row_hash{'nav_type'};
            my($lat) = $row_hash{'lat'};
            my($lng) = $row_hash{'lng'};
            my($elevation) = $row_hash{'elevation'};
            my($freq) = $row_hash{'freq'};
            my($range) = $row_hash{'range'};
            my($multi) = $row_hash{'multi'};
            my($ident) = $row_hash{'ident'};
            my($name) = &htmlencode($row_hash{'name'});

            my($nav_tag);
            if($nav_type eq '2')
            {
                $nav_tag = "ndb";
            }
            elsif($nav_type eq '3')
            {
                $nav_tag = "vor";
                $freq = &freqstr($freq);
            }

            $xml .= <<XML;
\t<${nav_tag} nav_type="${nav_type}" lat="${lat}" lng="${lng}" elevation="${elevation}" freq="${freq}" range="${range}" multi="${multi}" ident="${ident}" name="${name}" />"
XML
        }
        $result_cnt += $sth->rows;
    }

}


if($fix)
{
    $sql = "SELECT * FROM ${FIX_TABLE} WHERE ";

    if($sstr)
    {
        $sql .= "UPPER(name) LIKE '\%".uc(${sstr})."\%'";
    }
    elsif($ne and $sw)
    {
        $sql .= "(lat < ${ne_lat}";
        $sql .= " AND lat > ${sw_lat}";
        $sql .= " AND abslng < ${ne_lng}";
        $sql .= " AND abslng > ${sw_lng})";
    }

    $sql .= ";";

    $sth = $dbi->process($sql);

    if($sth->rows > 0)
    {
        my($i);
        for($i = 0; $i < (&min(${SQL_LIMIT} - ${result_cnt}, $sth->rows)); $i++)
        {
            my($row_href) = $sth->fetchrow_hashref;
            my(%row_hash) = %$row_href;

            my($lat) = $row_hash{'lat'};
            my($lng) = $row_hash{'lng'};
            my($name) = &htmlencode($row_hash{'name'});

            $xml .= <<XML;
\t<fix lat="${lat}" lng="${lng}" name="${name}" />"
XML
        }
        $result_cnt += $sth->rows;
    }
}


if($sstr and $awy)
{
    $sql = "SELECT * FROM ${AWY_TABLE}";
    $sql .= " WHERE UPPER(seg_name) LIKE '\%".uc(${sstr})."\%'";
    $sql .= ";";

    $sth = $dbi->process($sql);

    if($sth->rows > 0)
    {
        my($i);
        for($i = 0; $i < (&min(${SQL_LIMIT} - ${result_cnt}, $sth->rows)); $i++)
        {
            my($row_href) = $sth->fetchrow_hashref;
            my(%row_hash) = %$row_href;

            my($name_start) = $row_hash{'name_start'};
            my($lat_start) = $row_hash{'lat_start'};
            my($lng_start) = $row_hash{'lng_start'};

            my($name_end) = $row_hash{'name_end'};
            my($lat_end) = $row_hash{'lat_end'};
            my($lng_end) = $row_hash{'lng_end'};

            my($enroute) = $row_hash{'enroute'};
            my($base) = $row_hash{'base'};
            my($top) = $row_hash{'top'};
            my($seg_name) = $row_hash{'seg_name'};

            $xml .= <<XML;
\t<awy name_start="${name_start}" lat="${lat_start}" lng="${lng_start}" name_end="${name_end}" lat="${lat_end}" lng="${lng_end}" enroute="${enroute}" base="${base}" top="${top}" seg_name="${seg_name}" />"
XML
        }
        $result_cnt += $sth->rows;
    }
}


print("<navaids cnt=\"${result_cnt}\">\n");
print($xml);
print("</navaids>\n\n");

exit(0);

