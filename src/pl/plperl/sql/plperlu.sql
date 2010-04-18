-- Use ONLY plperlu tests here. For plperl/plerlu combined tests
-- see plperl_plperlu.sql

-- Avoid need for custom_variable_classes = 'plperl'
LOAD 'plperl';

-- Test plperl.on_plperlu_init gets run
SET plperl.on_plperlu_init = '$_SHARED{init} = 42';
DO $$ warn $_SHARED{init} $$ language plperlu;

--
-- Test compilation of unicode regex - regardless of locale.
-- This code fails in plain plperl in a non-UTF8 database.
--
CREATE OR REPLACE FUNCTION perl_unicode_regex(text) RETURNS INTEGER AS $$
  return ($_[0] =~ /\x{263A}|happy/i) ? 1 : 0; # unicode smiley
$$ LANGUAGE plperlu;


-- check that we cant override signals
DO $do$ die "__DIE__ is defined" if($SIG{__DIE__}); $do$ LANGUAGE plperlu;

-- check that we cant modify env
DO $do$ $ENV{'plperlu_env_test'} = 'test'; $do$ LANGUAGE plperlu;
DO $do$ die "modified ENV" if($ENV{'plperlu_env_test'}); $do$ LANGUAGE plperlu;
