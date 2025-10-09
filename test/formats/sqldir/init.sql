
CREATE VIEW lnav_db.web_status AS
  SELECT group_concat(cs_uri_stem), sc_status FROM lnav_db.access_log group by sc_status;

INSERT into lnav_view_filters VALUES ('log', 5, 0, 'in', 'regex', 'credential status');
