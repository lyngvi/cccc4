/*
    CCCC - C and C++ Code Counter
    Copyright (C) 1994-2005 Tim Littlefair (tim_littlefair@hotmail.com)

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
*/
// cccc_htm.cc

// this file defines HTML output facilities for the CCCC project

#include "cccc.h"
#include "cccc_itm.h"
#include "cccc_htm.h"
#include "cccc_ver.h"

// I would love to use the C++ standard preprocessor
// directive #if here, but I have had reports before now
// of people who are using compilers which only support
// #ifdef.
#ifdef CCCC_CONF_W32VC
#include <direct.h>
#else
#ifdef CCCC_CONF_W32BC
#include <direct.h>
#else
#include <unistd.h>
#endif
#endif

#include <time.h>
#include <sys/stat.h>
#include "cccc_utl.h"

#ifndef COUNTOF
#  define COUNTOF(x) (sizeof(x)/sizeof(*(x)))
#endif

typedef std::map<string,Source_Anchor> source_anchor_map_t;
source_anchor_map_t source_anchor_map;


// class static data members
CCCC_Project* CCCC_Html_Stream::prjptr;
string CCCC_Html_Stream::outdir;
string CCCC_Html_Stream::libdir;

struct metric_description_t {
    const char* abbreviation;
    const char* name;
    const char* description;
};

static metric_description_t ProjectSummaryMetrics[] = {
        { COUNT_TAG_NUMBER_OF_MODULES, "Number of modules",
                "Number of non-trivial modules identified by the "
                "analyser.  Non-trivial modules include all classes, "
                "and any other module for which member functions are "
                "identified." },
        { COUNT_TAG_LINES_OF_CODE, "Lines of Code",
                "Number of non-blank, non-comment lines of source code "
                "counted by the analyser." },
        { COUNT_TAG_LINES_OF_COMMENT, "Lines of Comments",
                "Number of lines of comment identified by the analyser" },
        { COUNT_TAG_CYCLOMATIC_NUMBER, "McCabe's Cyclomatic Complexity",
                "A measure of the decision complexity of the functions "
                "which make up the program."
                "The strict definition of this measure is that it is "
                "the number of linearly independent routes through "
                "a directed acyclic graph which maps the flow of control "
                "of a subprogram.  The analyser counts this by recording "
                "the number of distinct decision outcomes contained "
                "within each function, which yields a good approximation "
                "to the formally defined version of the measure." },
        { "L_C","Lines of code per line of comment",
                "Indicates density of comments with respect to textual "
                "size of program" },
        { "M_C","Cyclomatic Complexity per line of comment",
                "Indicates density of comments with respect to logical "
                "complexity of program" },
        { COUNT_TAG_INTERMODULE_COMPLEXITY4, "Information Flow measure",
                "Measure of information flow between modules suggested "
                "by Henry and Kafura. The analyser makes an approximate "
                "count of this by counting inter-module couplings "
                "identified in the module interfaces." },
        { "LOCpM", "Lines of Code per Method",
                "The average number of lines of code per method. "
                "High LoC count may indicate poor functional isolation. "
                "Note that this measure may be weighted low by large numbers of small accessor methods." },
        { "MLOCpM", "Max Lines of Code per Method",
                "The largest number of lines of code in a single method. "
                "High LoC count may indicate poor functional isolation." },
};

#define _WMC_DESCRIPTION_START "The sum of a weighting function over the functions of the module. "
#define _WMC_NAME_START "Weighted methods per class"
static metric_description_t OODesignMetrics[] = {
        { COUNT_TAG_WEIGHTED_METHODS_PER_CLASS_UNITY, _WMC_NAME_START " (all)",
                _WMC_DESCRIPTION_START "WMC1 uses the nominal weight of 1 for each "
                "function, and hence measures the number of functions." },
        { COUNT_TAG_WEIGHTED_METHODS_PER_CLASS COUNT_TAG_VISIBLE_SUFFIX, _WMC_NAME_START " (visible)",
                _WMC_DESCRIPTION_START "WMCv uses a weighting function which is 1 for functions "
                "accessible to other modules, 0 for private functions."},
        { COUNT_TAG_INHERITANCE_TREE_DEPTH, "Depth of inheritance tree",
                "The length of the longest path of inheritance ending at "
                "the current module.  The deeper the inheritance tree "
                "for a module, the harder it may be to predict its "
                "behaviour.  On the other hand, increasing depth gives "
                "the potential of greater reuse by the current module "
                "of behaviour defined for ancestor classes." },
        { COUNT_TAG_NUMBER_OF_CHILDREN, "Number of children",
                "The number of modules which inherit directly from the "
                "current module.  Moderate values of this measure "
                "indicate scope for reuse, however high values may "
                "indicate an inappropriate abstraction in the design." },
        { COUNT_TAG_COUPLING_BETWEEN_OBJECTS, "Coupling between objects",
                "The number of other modules which are coupled to the "
                "current module either as a client or a supplier. "
                "Excessive coupling indicates weakness of module "
                "encapsulation and may inhibit reuse." }
};

static metric_description_t StructuralSummaryMetrics[] = {
        { "Fan-in", "",
                   "The number of other modules which pass information "
                   "into the current module." },
        { "Fan-out", "",
                   "The number of other modules into which the current "
                   "module passes information"},
        { "IF4","Information Flow measure",
                   "A composite measure of structural complexity, "
                   "calculated as the square of the product of the fan-in "
                   "and fan-out of a single module.  Proposed by Henry and "
                   "Kafura."}
};

void CCCC_Html_Stream::GenerateReports(CCCC_Project* prj,
				       int report_mask,
				       const string& file,
				       const string& dir)
{
  prjptr=prj;
  outdir=dir;

  CCCC_Html_Stream main_html_stream(file.c_str(),"Report on software metrics");

  if(report_mask & rtCONTENTS)
    {
      // For testing purposes, we want to be able to disable the inclusion
      // of the current time in the report.  This enables us to store a
      // reference version of the report in RCS and expect the program
      // to generate an identical one at regression testing time.
      if(report_mask & rtSHOW_GEN_TIME)
	{
	  main_html_stream.Table_Of_Contents(report_mask,true);
	}
      else
	{
	  main_html_stream.Table_Of_Contents(report_mask,false);
	}
    }

  if(report_mask & rtSUMMARY)
    {
      main_html_stream.Project_Summary();
    }

  if(report_mask & rtPROC1)
    {
      main_html_stream.Procedural_Summary();
    }

  if(report_mask & rtPROC2)
    {
      main_html_stream.Procedural_Detail();
    }

  if(report_mask & rtOODESIGN)
    {
      main_html_stream.OO_Design();
    }

  if(report_mask & rtSTRUCT1)
    {
      main_html_stream.Structural_Summary();
    }

  if(report_mask & rtSTRUCT2)
    {
      main_html_stream.Structural_Detail();
    }

  if(report_mask & rtSEPARATE_MODULES)
    {
      main_html_stream.Separate_Modules();
    }

  if(report_mask & rtOTHER)
    {
      main_html_stream.Other_Extents();
    }

  if(report_mask & rtSOURCE)
    {
      main_html_stream.Source_Listing();
    }


  if(report_mask & rtCCCC)
    {
      main_html_stream.Put_Section_Heading("About CCCC","infocccc",1);
      main_html_stream.fstr <<
        HTMLParagraph("This report was generated by the program CCCC, which is FREELY "
		"REDISTRIBUTABLE but carries NO WARRANTY.") <<
        HTMLParagraph("CCCC was developed by Tim Littlefair. "
                "as part of a PhD research project. "
                "This project is now completed and descriptions of the "
                "findings can be accessed at "
                "<a href=\"http://www.chs.ecu.edu.au/~tlittlef\">"
                "http://www.chs.ecu.edu.au/~tlittlef</a>.") <<
        HTMLParagraph("User support for CCCC can be obtained by "
                "<a href=\"mailto:cccc-users@lists.sourceforge.net\">"
                "mailing the list cccc-users@lists.sourceforge.net</a>.") <<
        HTMLParagraph("Please also visit the CCCC development website at "
                "<a href=\"http://cccc.sourceforge.net\">http://cccc.sourceforge.net</a>.") <<
        HTMLParagraph("The CCCC4 fork adds some interactivity to the HTML rendering of CCCC's results, and the additional fields "
                "LOCpM and MLOCpM; its intent is to make the HTML output more suited for immediate presentation "
                "in a build environment e such as Jenkins. Visit the CCCC4 fork at <a href=\"http://www.github.com/lyngvi/cccc4\">http://www.github.com/lyngvi/cccc4</a>.");
    }
}

CCCC_Html_Stream::~CCCC_Html_Stream()
{
  fstr << "</BODY></HTML>" << endl;
  fstr.close();
}

void CCCC_Html_Stream::Table_Of_Contents(int report_mask, bool showGenTime)
{
  // record the number of report parts in the table, and the
  // stream put pointer
  // if we find that we have only generated a single part, we supress
  // the TOC by seeking to the saved stream offset
  int number_of_report_parts=0;
  int saved_stream_offset=fstr.tellp();

  fstr << HTMLBeginElement(_Table, "toc") << endl
       << HTMLBeginElement(_TableRow) << "<th colspan=\"2\">" << endl
       << "CCCC Software Metrics Report";
  if( prjptr->name(nlSIMPLE)!="" )
    {
      fstr << " on project " << prjptr->name(nlSIMPLE);
    }
  fstr << endl;

  // we have the option to disable the display of the generation time
  // so that we can generate identical reports for regression testing
  if(showGenTime==true)
    {
      time_t generationTime=time(NULL);
      fstr << _HTMLLineBreak
           << " generated " << ctime(&generationTime) << " by CCCC v" CCCC_VERSION_STRING << endl;
    }

  fstr << HTMLEndElement(_TableRow) << endl;

  if(report_mask & rtSUMMARY)
    {
      Put_Section_TOC_Entry(
			    "Project Summary","projsum",
			    "Summary table of high level measures summed "
			    "over all files processed in the current run.");
      number_of_report_parts++;
    }

  if(report_mask & rtPROC1)
    {
      Put_Section_TOC_Entry(
			    "Procedural Metrics Summary", "procsum",
			    "Table of procedural measures (i.e. lines of "
			    "code, lines of comment, McCabe's cyclomatic "
			    "complexity summed over each module.");
      number_of_report_parts++;
    }

  if(report_mask & rtPROC2)
    {
      Put_Section_TOC_Entry(
			    "Procedural Metrics Detail", "procdet",
			    "The same procedural metrics as in the procedural "
			    "metrics summary, reported for individual "
			    "functions, grouped by module.");
      number_of_report_parts++;
    }

  if(report_mask & rtOODESIGN)
    {
      Put_Section_TOC_Entry(
			    "Object Oriented Design","oodesign",
			    "Table of four of the 6 metrics proposed by "
			    "Chidamber and Kemerer in their various papers on "
			    "'a metrics suite for object oriented design'.");
      number_of_report_parts++;
    }

  if(report_mask & rtSTRUCT1)
    {
      Put_Section_TOC_Entry(
			    "Structural Metrics Summary", "structsum",
			    "Structural metrics based on the relationships of "
			    "each module with others.  Includes fan-out (i.e. "
			    "number of other modules the current module "
			    "uses), fan-in (number of other modules which use "
			    "the current module), and the Information Flow "
			    "measure suggested by Henry and Kafura, which "
			    "combines these to give a measure of coupling for "
			    "the module.");
      number_of_report_parts++;
    }

  if(report_mask & rtSTRUCT2)
    {
      Put_Section_TOC_Entry(
			    "Structural Metrics Detail", "structdet",
			    "The names of the modules included as clients and "
			    "suppliers in the counts for the Structural "
			    "Metrics Summary.");
      number_of_report_parts++;
    }

  if(report_mask & rtOTHER)
    {
      Put_Section_TOC_Entry(
			    "Other Extents", "other",
			    "Lexical counts for parts of submitted source "
			    "files which the analyser was unable to assign to "
			    "a module. Each record in this table relates to "
			    "either a part of the code which triggered a "
			    "parse failure, or to the residual lexical counts "
			    "relating to parts of a file not associated with "
			    "a specific module."
			    );
      number_of_report_parts++;
    }

  if(report_mask & rtCCCC)
    {
      Put_Section_TOC_Entry(
			    "About CCCC", "infocccc",
			    "A description of the CCCC program.");
      number_of_report_parts++;
    }

  fstr << HTMLEndElement(_TableRow) << HTMLEndElement(_Table) << endl;
  if(number_of_report_parts<2)
    {
      fstr.seekp(saved_stream_offset);
    }
}

void CCCC_Html_Stream::Put_Section_Heading(
					   string heading_title,
					   string heading_tag,
					   int heading_level)
{
  fstr << "<a name=\"" << heading_tag << "\"></a>"
       << "<h" << heading_level << ">"
       << heading_title
       << "</h" << heading_level << ">" << endl;
}

void  CCCC_Html_Stream::Project_Summary() {
  Put_Section_Heading("Project Summary","projsum",1);

  fstr << "<p>This table shows measures over the project as a whole.</p>" << endl;

  fstr << HTMLBeginElement(_UnorderedList, "Project_Summary") << endl;
  for (int k = 0; k < COUNTOF(ProjectSummaryMetrics); ++k)
      Metric_Description(ProjectSummaryMetrics[k].abbreviation, ProjectSummaryMetrics[k].name, ProjectSummaryMetrics[k].description);
  fstr << HTMLEndElement(_UnorderedList) << endl
       << HTMLParagraph(
    	  "Two variants on the information flow measure IF4 are also "
    	  "presented, one (IF4v) calculated using only relationships in the "
          "visible part of the module interface, and the other (IF4c) "
          "calculated using only those relationships which imply that changes "
          "to the client must be recompiled of the supplier's definition "
          "changes.")
       << endl << endl;

  // calculate the counts on which all displayed data will be based
  int nom=prjptr->get_count(COUNT_TAG_NUMBER_OF_MODULES);
  int loc=prjptr->get_count(COUNT_TAG_LINES_OF_CODE);
  int mvg=prjptr->get_count(COUNT_TAG_CYCLOMATIC_NUMBER);
  int com=prjptr->get_count(COUNT_TAG_LINES_OF_COMMENT);
  int if4=prjptr->get_count(COUNT_TAG_INTERMODULE_COMPLEXITY4);
  int if4v=prjptr->get_count(COUNT_TAG_INTERMODULE_COMPLEXITY4 COUNT_TAG_VISIBLE_SUFFIX);
  int if4c=prjptr->get_count(COUNT_TAG_INTERMODULE_COMPLEXITY4 COUNT_TAG_CONCRETE_SUFFIX);
  int rej=prjptr->rejected_extent_table.get_count(COUNT_TAG_LINES_OF_CODE);

  fstr << HTMLBeginElement(_Table, "summary")
       << HTMLBeginElement(_TableHead) << endl
       << HTMLBeginElement(_TableRow) << endl;
  Put_Header_Cell("Metric",55);
  Put_Header_Cell("Tag",15);
  Put_Header_Cell("Overall",15);
  Put_Header_Cell("Per Module",15);
  fstr << HTMLEndElement(_TableRow) << endl
       << HTMLEndElement(_TableHead) << endl;

  fstr << HTMLBeginElement(_TableRow) << endl;
  Put_Label_Cell("Number of modules");
  Put_Label_Cell(COUNT_TAG_NUMBER_OF_MODULES);
  Put_Metric_Cell(nom);
  fstr << HTMLTableCell("&nbsp;");
  fstr << HTMLEndElement(_TableRow) << endl;

  fstr << HTMLBeginElement(_TableRow) << endl;
  Put_Label_Cell("Lines of Code");
  Put_Label_Cell(COUNT_TAG_LINES_OF_CODE);
  Put_Metric_Cell(loc,"LOCp");
  Put_Metric_Cell(loc,nom,"LOCper");
  fstr << HTMLEndElement(_TableRow) << endl;

  fstr << HTMLBeginElement(_TableRow) << endl;
  Put_Label_Cell("McCabe's Cyclomatic Number");
  Put_Label_Cell(COUNT_TAG_CYCLOMATIC_NUMBER);
  Put_Metric_Cell(mvg,"MVGp");
  Put_Metric_Cell(mvg,nom,"MVGper");
  fstr << HTMLEndElement(_TableRow) << endl;

  fstr << HTMLBeginElement(_TableRow) << endl;
  Put_Label_Cell("Lines of Comment");
  Put_Label_Cell(COUNT_TAG_LINES_OF_COMMENT);
  Put_Metric_Cell(com,"COM");
  Put_Metric_Cell(com,nom,"COMper");
  fstr << HTMLEndElement(_TableRow) << endl;

  fstr << HTMLBeginElement(_TableRow) << endl;
  Put_Label_Cell("LOC/COM");
  Put_Label_Cell("L_C");
  Put_Metric_Cell(loc,com,"L_C");
  fstr << HTMLTableCell("");
  fstr << HTMLEndElement(_TableRow) << endl;

  fstr << HTMLBeginElement(_TableRow) << endl;
  Put_Label_Cell("MVG/COM");
  Put_Label_Cell("M_C");
  Put_Metric_Cell(mvg,com,"M_C");
  fstr << HTMLTableCell("");
  fstr << HTMLEndElement(_TableRow) << endl;

  fstr << HTMLBeginElement(_TableRow) << endl;
  Put_Label_Cell("Information Flow measure (inclusive)");
  Put_Label_Cell(COUNT_TAG_INTERMODULE_COMPLEXITY4);
  Put_Metric_Cell(if4);
  Put_Metric_Cell(if4,nom,"8.3");
  fstr << HTMLEndElement(_TableRow) << endl;

  fstr << HTMLBeginElement(_TableRow) << endl;
  Put_Label_Cell("Information Flow measure (visible)");
  Put_Label_Cell(COUNT_TAG_INTERMODULE_COMPLEXITY4 COUNT_TAG_VISIBLE_SUFFIX);
  Put_Metric_Cell(if4v);
  Put_Metric_Cell(if4v,nom,"8.3");
  fstr << HTMLEndElement(_TableRow) << endl;

  fstr << HTMLBeginElement(_TableRow) << endl;
  Put_Label_Cell("Information Flow measure (concrete)");
  Put_Label_Cell(COUNT_TAG_INTERMODULE_COMPLEXITY4 COUNT_TAG_CONCRETE_SUFFIX);
  Put_Metric_Cell(if4c);
  Put_Metric_Cell(if4c,nom,"8.3");
  fstr << HTMLEndElement(_TableRow) << endl;

  fstr << HTMLBeginElement(_TableRow) << endl;
  Put_Label_Cell("Lines of Code rejected by parser");
  Put_Label_Cell("REJ");
  Put_Metric_Cell(rej,"REJ");
  fstr << HTMLTableCell("");
  fstr << HTMLEndElement(_TableRow) << endl;

  fstr << HTMLEndElement(_Table) << endl;
}

void CCCC_Html_Stream::OO_Design() {
  Put_Section_Heading("Object Oriented Design","oodesign",1);
  int metric_tag_count = COUNTOF(OODesignMetrics);

  fstr << HTMLBeginElement(_UnorderedList) << endl;
  for (size_t k = 0; k < metric_tag_count; ++k)
      Metric_Description(OODesignMetrics[k].abbreviation, OODesignMetrics[k].name, OODesignMetrics[k].description);
  fstr << HTMLEndElement(_UnorderedList) << endl << endl;

  fstr << HTMLParagraph(
		  "The label cell for each row in this table provides a link to "
		  "the module summary table in the detailed report for the "
		  "module in question.") << endl;


  fstr << HTMLBeginElement(_Table, "summary") << endl
       << HTMLBeginElement(_TableHead) << endl
       << HTMLBeginElement(_TableRow) << endl;

  Put_Header_Cell("Module Name", 100 - 10 * metric_tag_count);
  for (size_t k = 0; k < metric_tag_count; ++k)
      Put_Header_Cell(OODesignMetrics[k].abbreviation, 10);

  fstr << HTMLEndElement(_TableRow)
       << HTMLEndElement(_TableHead) << endl;

  CCCC_Module* mod_ptr=prjptr->module_table.first_item();
  int i=0;
  while(mod_ptr!=NULL)
    {
      i++;
      if( mod_ptr->is_trivial() == FALSE)
	{
	  fstr << HTMLBeginElement(_TableRow) << endl;

	  string href=mod_ptr->key()+".html#summary";

	  Put_Label_Cell(mod_ptr->name(nlSIMPLE).c_str(),0,"",href.c_str());

	  for(size_t j=0; j < metric_tag_count; j++)
	    {
	      CCCC_Metric metric_value(mod_ptr->get_count(OODesignMetrics[j].abbreviation), OODesignMetrics[j].abbreviation);
	      Put_Metric_Cell(metric_value);
	    }
	  fstr << HTMLEndElement(_TableRow) << endl;

	}
      mod_ptr=prjptr->module_table.next_item();
    }

  fstr << HTMLEndElement(_Table) << endl;

}

void CCCC_Html_Stream::Procedural_Summary() {
  Put_Section_Heading("Procedural Metrics Summary","procsum",1);

  fstr << HTMLParagraph(
		  "For descriptions of each of these metrics see the information "
		  "preceding the project summary table.") << endl << endl;

  fstr << HTMLParagraph(
		  "The label cell for each row in this table provides a link to "
		  "the functions table in the detailed report for the "
		  "module in question") << endl;

  fstr << HTMLBeginElement(_Table, "summary")
       << HTMLBeginElement(_TableHead)
       << HTMLBeginElement(_TableRow) << endl;
  Put_Header_Cell("Module Name");
  Put_Header_Cell("LOC",8);
  Put_Header_Cell("MVG",8);
  Put_Header_Cell("COM",8);
  Put_Header_Cell("L_C",8);
  Put_Header_Cell("M_C",8);
  Put_Header_Cell("LOCpM",8);
  Put_Header_Cell("MLOCpM", 8);

  fstr << HTMLEndElement(_TableRow)
	   << HTMLEndElement(_TableHead) << endl;

  CCCC_Module* mod_ptr=prjptr->module_table.first_item();
  int i=0;
  while(mod_ptr!=NULL)
    {
      i++;
      if( mod_ptr->is_trivial() == FALSE)
	{
	  fstr << HTMLBeginElement(_TableRow) << endl;
	  string href=mod_ptr->key()+".html#procdet";

	  Put_Label_Cell(mod_ptr->name(nlSIMPLE).c_str(),0,"",href.c_str());
	  int loc=mod_ptr->get_count(COUNT_TAG_LINES_OF_CODE);
	  int mvg=mod_ptr->get_count(COUNT_TAG_CYCLOMATIC_NUMBER);
	  int com=mod_ptr->get_count(COUNT_TAG_LINES_OF_COMMENT);

	  Put_Metric_Cell(CCCC_Metric(loc, "LOCm"));
	  Put_Metric_Cell(CCCC_Metric(mvg, "MVGm"));
	  Put_Metric_Cell(com);
	  Put_Metric_Cell(CCCC_Metric(loc, com, "L_C"));
	  Put_Metric_Cell(CCCC_Metric(mvg, com, "M_C"));
	  Put_Metric_Cell(CCCC_Metric(loc, mod_ptr->get_count(COUNT_TAG_WEIGHTED_METHODS_PER_CLASS_UNITY), "LOCf"));
          Put_Metric_Cell(CCCC_Metric(mod_ptr->get_count(COUNT_TAG_MAX_LINES_OF_CODE_PER_METHOD), "LOCf"));

	  fstr << HTMLEndElement(_TableRow) << endl;

	}
      mod_ptr=prjptr->module_table.next_item();
    }

  fstr << HTMLEndElement(_Table) << endl;

}

void CCCC_Html_Stream::Structural_Summary()
{
  Put_Section_Heading("Structural Metrics Summary","structsum",1);

  fstr << HTMLBeginElement(_UnorderedList) << endl;
  for (int k = 0; k < COUNTOF(StructuralSummaryMetrics); ++k)
      Metric_Description(StructuralSummaryMetrics[k].abbreviation, StructuralSummaryMetrics[k].name, StructuralSummaryMetrics[k].description);
  fstr << HTMLEndElement(_UnorderedList) << endl;

  fstr << HTMLParagraph("Note that the fan-in and fan-out are calculated by examining the "
		  "interface of each module.  As noted above, three variants of each "
		  "each of these measures are presented: a count restricted to the "
		  "part of the interface which is externally visible, a count which "
		  "only includes relationships which imply the client module needs "
		  "to be recompiled if the supplier's implementation changes, and an "
		  "inclusive count") << endl << endl;


  fstr << HTMLParagraph("The label cell for each row in this table provides a link to "
		  "the relationships table in the detailed report for the "
		  "module in question") << endl << endl;

  fstr  << HTMLBeginElement(_Table, "summary") << endl
        << HTMLBeginElement(_TableHead) << endl
	<< HTMLBeginElement(_TableRow) << endl
	<< "<th rowspan=\"2\" class=\"sortable\"><div>Module Name</div></td>" << endl
	<< HTMLMultiColumnHeaderCell("Fan-out", 3) << endl
	<< HTMLMultiColumnHeaderCell("Fan-in", 3) << endl
	<< HTMLMultiColumnHeaderCell("IF4", 3) << endl
	<< HTMLEndElement(_TableRow) << endl
	<< HTMLBeginElement(_TableRow) << endl;
  Put_Header_Cell("vis",7);
  Put_Header_Cell("con",7);
  Put_Header_Cell("inc",7);
  Put_Header_Cell("vis",7);
  Put_Header_Cell("con",7);
  Put_Header_Cell("incl",7);
  Put_Header_Cell("vis",7);
  Put_Header_Cell("con",7);
  Put_Header_Cell("inc",7);
  fstr << HTMLEndElement(_TableRow)
       << HTMLEndElement(_TableHead) << endl;


  CCCC_Module* module_ptr=prjptr->module_table.first_item();
  while(module_ptr!=NULL)
    {
      if(module_ptr->is_trivial()==FALSE)
	{
	  fstr << HTMLBeginElement(_TableRow) << endl;

	  int fov=module_ptr->get_count(COUNT_TAG_FAN_OUT COUNT_TAG_VISIBLE_SUFFIX);
	  int foc=module_ptr->get_count(COUNT_TAG_FAN_OUT COUNT_TAG_CONCRETE_SUFFIX);
	  int fo=module_ptr->get_count(COUNT_TAG_FAN_OUT);

	  int fiv=module_ptr->get_count(COUNT_TAG_FAN_IN COUNT_TAG_VISIBLE_SUFFIX);
	  int fic=module_ptr->get_count(COUNT_TAG_FAN_IN COUNT_TAG_CONCRETE_SUFFIX);
	  int fi=module_ptr->get_count(COUNT_TAG_FAN_IN);

	  int if4v=module_ptr->get_count(COUNT_TAG_INTERMODULE_COMPLEXITY4 COUNT_TAG_VISIBLE_SUFFIX);
	  int if4c=module_ptr->get_count(COUNT_TAG_INTERMODULE_COMPLEXITY4 COUNT_TAG_CONCRETE_SUFFIX);
	  int if4=module_ptr->get_count(COUNT_TAG_INTERMODULE_COMPLEXITY4);

	  // the last two arguments here turn on links to enable jumping between
	  // the summary and detail cells for the same module
	  string href=module_ptr->key()+".html#structdet";
	  Put_Label_Cell(module_ptr->name(nlSIMPLE).c_str(), 0, "",href.c_str());
	  Put_Metric_Cell(CCCC_Metric(fov,"FOv"));
	  Put_Metric_Cell(CCCC_Metric(foc,"FOc"));
	  Put_Metric_Cell(CCCC_Metric(fo,"FO"));
	  Put_Metric_Cell(CCCC_Metric(fiv,"FIv"));
	  Put_Metric_Cell(CCCC_Metric(fic,"FIc"));
	  Put_Metric_Cell(CCCC_Metric(fi,"FI"));
	  Put_Metric_Cell(CCCC_Metric(if4v,"IF4v"));
	  Put_Metric_Cell(CCCC_Metric(if4c,"IF4c"));
	  Put_Metric_Cell(CCCC_Metric(if4,"IF4"));

	  fstr << HTMLEndElement(_TableRow) << endl;
	}
      module_ptr=prjptr->module_table.next_item();
    }
  fstr << HTMLEndElement(_Table) << endl;

}

void CCCC_Html_Stream::Put_Structural_Details_Cell(
						   CCCC_Module *mod, CCCC_Project *prj, int mask, UserelNameLevel nl)
{
  fstr << HTMLBeginElement(_TableCell, "", 50) << endl;

#if 0
  std::cerr << "Relationships for " << mod->name(nlMODULE_NAME)
	    << " (" << mod << ")" << std::endl;
#endif

  CCCC_Module::relationship_map_t::iterator iter;
  CCCC_Module::relationship_map_t *relationship_map=NULL;
  if(mask==rmeCLIENT)
    {
      relationship_map=&(mod->client_map);
    }
  else if(mask==rmeSUPPLIER)
    {
      relationship_map=&(mod->supplier_map);
    }

  if(relationship_map==NULL)
    {
      cerr << "unexpected relationship mask " << mask  << endl;
    }
  else
    {
      for(
	  iter=relationship_map->begin();
	  iter!=relationship_map->end();
	  iter++
	  )
	{
	  CCCC_UseRelationship *ur_ptr=(*iter).second;
	  fstr << ur_ptr->name(nl) << " ";
	  AugmentedBool vis=ur_ptr->is_visible();
	  AugmentedBool con=ur_ptr->is_concrete();

#if 0
	  std::cerr << ur_ptr->name(nlCLIENT)
		    << " uses "
		    << ur_ptr->name(nlSUPPLIER)
		    << std::endl;
#endif

	  if( (vis != abFALSE) && (con != abFALSE) )
	    {
	      fstr << "[CV] ";
	    }
	  else if(vis != abFALSE)
	    {
	      fstr << "[V] ";
	    }
	  else if(con != abFALSE)
	    {
	      fstr << "[C] ";
	    }
	  fstr << _HTMLLineBreak << endl;
	  Put_Extent_List(*ur_ptr,true);
	  fstr << _HTMLLineBreak << endl;
	}
    }
  // put a non-breaking space in to avoid the unpleasantness which
  // goes with completely empty cells
  fstr << "&nbsp;" << endl;

  fstr << HTMLEndElement(_TableCell) << endl;

}

void CCCC_Html_Stream::Structural_Detail()
{
  Put_Section_Heading("Structural Metrics Detail","structdet",1);
  fstr << HTMLBeginElement(_Table, "summary")
		  << HTMLBeginElement(_TableHead)
		  << HTMLBeginElement(_TableRow) << endl;
  Put_Header_Cell("Module Name",20);
  Put_Header_Cell("Clients",40);
  Put_Header_Cell("Suppliers",40);
  fstr << HTMLEndElement(_TableHead)
		  << HTMLEndElement(_TableRow) << endl;

  CCCC_Module* module_ptr=prjptr->module_table.first_item();
  while(module_ptr!=NULL)
    {
      if(module_ptr->is_trivial()==FALSE)
	{
	  fstr << HTMLBeginElement(_TableRow) << endl;
	  Put_Label_Cell(module_ptr->name(nlSIMPLE).c_str(), 0, "structdet","structsum");
	  Structural_Detail(module_ptr);
	  fstr << HTMLEndElement(_TableRow) << endl;
	}
      module_ptr=prjptr->module_table.next_item();
    }
  fstr << HTMLEndElement(_Table) << endl;

}

void CCCC_Html_Stream::Procedural_Detail() {
  Put_Section_Heading("Procedural Metrics Detail","procdet",1);

  fstr << HTMLBeginElement(_Table, "summary") << endl;

  CCCC_Module* mod_ptr=prjptr->module_table.first_item();
  while(mod_ptr!=NULL)
    {
      if(
	 (mod_ptr->name(nlMODULE_TYPE)!="builtin") &&
	 (mod_ptr->name(nlMODULE_TYPE)!="enum") &&
	 (mod_ptr->name(nlMODULE_TYPE)!="union")
	 )
	{
	  fstr << HTMLBeginElement(_TableRow) << endl;
	  Put_Label_Cell(mod_ptr->name(nlSIMPLE).c_str(),50,
			 "procdet","procsum",mod_ptr);
	  Put_Header_Cell("LOC",10);
	  Put_Header_Cell("MVG",10);
	  Put_Header_Cell("COM",10);
	  Put_Header_Cell("L_C",10);
	  Put_Header_Cell("M_C",10);

	  fstr << HTMLEndElement(_TableRow) << endl;
	  Procedural_Detail(mod_ptr);
	}
      mod_ptr=prjptr->module_table.next_item();
    }
  fstr << HTMLEndElement(_Table) << endl;
}

void CCCC_Html_Stream::Other_Extents()
{
  Put_Section_Heading("Other Extents","other",1);
  fstr << HTMLBeginElement(_Table, "summary") << endl
  	   << HTMLBeginElement(_TableHead) << endl
  	   << HTMLBeginElement(_TableRow) << endl;
  Put_Header_Cell("Location",25);
  Put_Header_Cell("Text",45);
  Put_Header_Cell("LOC",10);
  Put_Header_Cell("COM",10);
  Put_Header_Cell("MVG",10);
  fstr << HTMLEndElement(_TableRow) << endl
       << HTMLEndElement(_TableHead) << endl;

  if(prjptr->rejected_extent_table.records() == 0)
    {
      fstr << HTMLSingleEntryRow(5, "&nbsp;") << endl;
    }
  else
    {
      CCCC_Extent *extent_ptr=prjptr->rejected_extent_table.first_item();
      while(extent_ptr!=NULL)
	{
	  fstr << HTMLBeginElement(_TableRow);
	  Put_Extent_Cell(*extent_ptr,0);
	  fstr << HTMLTableCell(HTMLEscapeLiteral(extent_ptr->name(nlDESCRIPTION).c_str()).c_str());
	  Put_Metric_Cell(extent_ptr->get_count(COUNT_TAG_LINES_OF_CODE),"");
	  Put_Metric_Cell(extent_ptr->get_count(COUNT_TAG_LINES_OF_COMMENT),"");
	  Put_Metric_Cell(extent_ptr->get_count(COUNT_TAG_CYCLOMATIC_NUMBER),"");
	  fstr << HTMLEndElement(_TableRow) << endl;
	  extent_ptr=prjptr->rejected_extent_table.next_item();
	}
    }
  fstr << HTMLEndElement(_Table) << endl;

}

void CCCC_Html_Stream::Put_Section_TOC_Entry(
					     string section_name, string section_href,
					     string section_description)
{
  fstr << HTMLBeginElement(_TableRow) << endl
       << HTMLBeginElement(_TableCell, "toc_entry_name", 20)
       << "<a href=\"#" << section_href << "\">" << section_name << "</a>"
       << HTMLEndElement(_TableHeader)
       << HTMLBeginElement(_TableCell)
       << section_description << endl
       << HTMLEndElement(_TableRow) << endl;
}

void CCCC_Html_Stream::Put_Header_Cell(string label, int width)
{
  fstr  << HTMLBeginElement(_TableHeader, "header_cell sortable", width)
        << HTMLBeginElement(_Div) << HTMLEscapeLiteral(label.c_str()) << HTMLEndElement(_Div)
        << HTMLEndElement(_TableHeader) << endl;
}

void CCCC_Html_Stream::Put_Label_Cell(
				      string label, int width,
				      string ref_name, string ref_href,
				      CCCC_Record *rec_ptr)
{
  fstr << HTMLBeginElement(_TableCell, "label", width);

  if(ref_name.size() > 0)
    {
      // we need to insert an HTML "<A NAME=...> tag for the current cell
      // this enables other locations to jump in
      fstr << "<a name=\"" << ref_name << "\"></a>" << endl;
    }

  if(ref_href.size() > 0)
    {
      // we need to insert an HTML <A HREF=...> tag for the current cell
      // this enables this cell to be a link to jump out
      fstr << "<a href=\"" << ref_href << "\">" << endl;
      // this anchor will need to be closed after the label has been displayed
    }

  fstr << label;

  if(ref_href.size() > 0)
    {
      // closing the anchor we opened above
      fstr << "</a>" << endl;
    }

  if(rec_ptr != 0)
    {
      fstr << _HTMLLineBreak << endl;
      Put_Extent_List(*rec_ptr,true);
    }

  fstr << HTMLEndElement(_TableCell);
}


void CCCC_Html_Stream::Put_Metric_Cell(
				       int count, string tag, int width)
{
  CCCC_Metric m(count, tag.c_str());
  Put_Metric_Cell(m, width);
}

void CCCC_Html_Stream::Put_Metric_Cell(
				       int num, int denom, string tag, int width)
{
  CCCC_Metric m(num,denom, tag.c_str());
  Put_Metric_Cell(m, width);
}

void  CCCC_Html_Stream::Put_Metric_Cell(const CCCC_Metric& metric, int width)
{
    const char* clasName = "metric";
    switch(metric.emphasis_level()) {
    case elLOW: 	clasName = "metric_low"; break;
    case elMEDIUM: 	clasName = "metric_medium"; break;
    case elHIGH: 	clasName = "metric_high"; break;
    }
    fstr << HTMLBeginElement(_TableCell, clasName, width);
    *this << metric;
    fstr << HTMLEndElement(_TableCell);
}

void CCCC_Html_Stream::Put_Extent_URL(const CCCC_Extent& extent)
{
  string filename=extent.name(nlFILENAME);
  int linenumber=atoi(extent.name(nlLINENUMBER).c_str());

  Source_Anchor anchor(filename, linenumber);
  string key=anchor.key();
  source_anchor_map_t::value_type anchor_value(key, anchor);
  source_anchor_map.insert(anchor_value);

  anchor.Emit_HREF(fstr);
  fstr
	// << extent.name(nlDESCRIPTION)
	<< _HTMLLineBreak << endl;
}

void CCCC_Html_Stream::Put_Extent_Cell(const CCCC_Extent& extent, int width, bool withDescription)
{
    fstr << HTMLBeginElement(_TableCell, "extent", width);
    if(withDescription)
        fstr << extent.name(nlDESCRIPTION) << " &nbsp;" << endl;
    Put_Extent_URL(extent);
    fstr << HTMLEndElement(_TableCell) << endl;
}

void CCCC_Html_Stream::Put_Extent_List(CCCC_Record& record, bool withDescription)
{
  CCCC_Extent *ext_ptr=record.extent_table.first_item();
  while(ext_ptr!=NULL)
    {
      if(withDescription)
      {
          fstr << ext_ptr->name(nlDESCRIPTION) << " &nbsp;" << endl;
      }
      Put_Extent_URL(*ext_ptr);
      ext_ptr=record.extent_table.next_item();
    }
  fstr << _HTMLLineBreak << endl;
}

CCCC_Html_Stream& operator <<(CCCC_Html_Stream& os, const string& stg)
	{ os.fstr << CCCC_Html_Stream::HTMLEscapeLiteral(stg.c_str()); }

// the next two methods define the two basic output operations through which
// all of the higher level output operations are composed

CCCC_Html_Stream& operator <<(CCCC_Html_Stream& os, const CCCC_Metric& mtc)
{
  const char *emphasis_prefix[]={"<span class=\"metric_low\">","<span class=\"metric_medium\">","<span class=\"metric_high\">"};
  const char *emphasis_suffix[]={"</span>","</span>","</span>"};

  // by writing to the underlying ostream object, we avoid the escape
  // functionality
  os.fstr << emphasis_prefix[mtc.emphasis_level()]
	  << mtc.value_string()
	  << emphasis_suffix[mtc.emphasis_level()];
  return os;
}

void CCCC_Html_Stream::Separate_Modules()
{
  // this function generates a separate HTML report for each non-trivial
  // module in the database

  CCCC_Module* mod_ptr=prjptr->module_table.first_item();
  while(mod_ptr!=NULL)
    {
      int trivial_module=mod_ptr->is_trivial();
      if(trivial_module==FALSE)
	{
	  string info="Detailed report on module " + mod_ptr->key();
	  string filename=outdir;
	  filename+="/";
	  filename+=mod_ptr->key()+".html";
	  CCCC_Html_Stream module_html_str(filename,info.c_str());

	  module_html_str.Put_Section_Heading(info.c_str(),"summary",1);

	  module_html_str.Module_Summary(mod_ptr);

	  module_html_str.Put_Section_Heading("Definitions and Declarations",
										  "modext",2);
	  module_html_str.fstr << HTMLBeginElement(_Table, "summary")
			  	  	  	   << HTMLBeginElement(_TableHead)
						   << HTMLBeginElement(_TableRow) << endl;
	  module_html_str.Put_Label_Cell("Description",50);
	  module_html_str.Put_Header_Cell("LOC",10);
	  module_html_str.Put_Header_Cell("MVG",10);
	  module_html_str.Put_Header_Cell("COM",10);
	  module_html_str.Put_Header_Cell("L_C",10);
	  module_html_str.Put_Header_Cell("M_C",10);
	  module_html_str.fstr
	  	  	  << HTMLEndElement(_TableRow)
			  << HTMLEndElement(_TableHead);
	  module_html_str.Module_Detail(mod_ptr);
	  module_html_str.fstr << HTMLEndElement(_Table);

	  module_html_str.Put_Section_Heading("Functions","proc",2);
	  module_html_str.fstr << HTMLBeginElement(_Table, "summary")
	  	  	  	  	  	   << HTMLBeginElement(_TableHead)
						   << HTMLBeginElement(_TableRow) << endl;
	  module_html_str.Put_Label_Cell("Function prototype",50);
	  module_html_str.Put_Header_Cell("LOC",10);
	  module_html_str.Put_Header_Cell("MVG",10);
	  module_html_str.Put_Header_Cell("COM",10);
	  module_html_str.Put_Header_Cell("L_C",10);
	  module_html_str.Put_Header_Cell("M_C",10);
	  module_html_str.fstr << HTMLEndElement(_TableRow)
						   << HTMLEndElement(_TableHead) << endl;
	  module_html_str.Procedural_Detail(mod_ptr);
	  module_html_str.fstr << HTMLEndElement(_Table);

	  module_html_str.Put_Section_Heading("Relationships","structdet",2);
	  module_html_str.fstr << HTMLBeginElement(_Table, "summary")
			  << HTMLBeginElement(_TableHead)
			  << HTMLBeginElement(_TableRow)
			  << HTMLBeginElement(_TableHeader, "", 50) << "Clients" << HTMLEndElement(_TableHeader)
			  << HTMLBeginElement(_TableHeader, "", 50) << "Suppliers" << HTMLEndElement(_TableHeader)
			  << HTMLEndElement(_TableRow) << endl
			  << HTMLEndElement(_TableHead) << endl
			  << HTMLBeginElement(_TableRow) << endl;
	  module_html_str.Structural_Detail(mod_ptr);
	  module_html_str.fstr << HTMLEndElement(_TableRow) << HTMLEndElement(_Table) << endl;


	}
      else
	{
#if 0
	  cerr << mod_ptr->module_type << " " << mod_ptr->key()
	       << " is trivial" << endl;
#endif
	}
      mod_ptr=prjptr->module_table.next_item();
    }
}

void CCCC_Html_Stream::Module_Detail(CCCC_Module *module_ptr)
{
  // this function generates the contents of the table of definition
  // and declaration extents for a single module

  // the output needs to be enveloped in a pair of <TABLE></TABLE> tags
  // these have not been put within the function because it is designed
  // to be used in two contexts:
  // 1. within the Separate_Modules function, wrapped directly in the table
  //    tags
  // 2. within the Module_Detail function, where the table tags are
  //    around the output of many calls to this function (not yet implemented)

  CCCC_Record::Extent_Table::iterator eIter = module_ptr->extent_table.begin();
  if(eIter==module_ptr->extent_table.end())
    {
      fstr 	<< HTMLSingleEntryRow(6, "No module extents have been identified for this module") << endl;
    }
  else
    {
      while(eIter!=module_ptr->extent_table.end())
	{
	  CCCC_Extent *ext_ptr=(*eIter).second;
	  fstr << HTMLBeginElement(_TableRow) << endl;
	  Put_Extent_Cell(*ext_ptr,0,true);
	  int loc=ext_ptr->get_count(COUNT_TAG_LINES_OF_CODE);
	  int mvg=ext_ptr->get_count(COUNT_TAG_CYCLOMATIC_NUMBER);
	  int com=ext_ptr->get_count(COUNT_TAG_LINES_OF_COMMENT);

	  Put_Metric_Cell(CCCC_Metric(loc, "LOCf"));
	  Put_Metric_Cell(CCCC_Metric(mvg, "MVGf"));
	  Put_Metric_Cell(com);
	  Put_Metric_Cell(CCCC_Metric(loc, com, "L_C"));
	  Put_Metric_Cell(CCCC_Metric(mvg, com, "M_C"));
	  fstr << HTMLEndElement(_TableRow) << endl;

	  eIter++;
	}
    }
}

void CCCC_Html_Stream::Procedural_Detail(CCCC_Module *module_ptr)
{
  // this function generates the contents of the procedural detail table
  // relating to a single module

  // the output needs to be enveloped in a pair of <TABLE></TABLE> tags
  // these have not been put within the function because it is designed
  // to be used in two contexts:
  // 1. within the Separate_Modules function, wrapped directly in the table
  //    tags
  // 2. within the Procedural_Detail function, where the table tags are
  //    around the output of many calls to this function

  CCCC_Module::member_map_t::iterator iter = module_ptr->member_map.begin();

  if(iter==module_ptr->member_map.end())
      fstr << HTMLSingleEntryRow(6, "No member functions have been identified for this module") << endl;
  else
    {
      while(iter!=module_ptr->member_map.end())
	{
	  CCCC_Member *mem_ptr=(*iter).second;
	  fstr << HTMLBeginElement(_TableRow) << endl;
	  Put_Label_Cell(mem_ptr->name(nlLOCAL).c_str(),0,"","",mem_ptr);
	  int loc=mem_ptr->get_count(COUNT_TAG_LINES_OF_CODE);
	  int mvg=mem_ptr->get_count(COUNT_TAG_CYCLOMATIC_NUMBER);
	  int com=mem_ptr->get_count(COUNT_TAG_LINES_OF_COMMENT);

	  Put_Metric_Cell(CCCC_Metric(loc, "LOCf"));
	  Put_Metric_Cell(CCCC_Metric(mvg, "MVGf"));
	  Put_Metric_Cell(com);
	  Put_Metric_Cell(CCCC_Metric(loc, com, "L_C"));
	  Put_Metric_Cell(CCCC_Metric(mvg, com, "M_C"));
	  fstr << HTMLEndElement(_TableRow) << endl;

	  iter++;
	}
    }
}

void CCCC_Html_Stream::Metric_Description(
					  string abbreviation,
					  string name,
					  string description)
{
  // this is intended to be called in the context of an unnumbered list
  fstr << HTMLBeginElement(_ListItem)
       << abbreviation;
  if (!name.empty())
      fstr  << " = " << name;
  fstr << HTMLParagraph(description.c_str()) << endl
       << HTMLEndElement(_ListItem) << endl;
}

void CCCC_Html_Stream::Structural_Detail(CCCC_Module *module_ptr)
{
  Put_Structural_Details_Cell(module_ptr, prjptr, rmeCLIENT, nlCLIENT);
  Put_Structural_Details_Cell(module_ptr, prjptr, rmeSUPPLIER, nlSUPPLIER);
}

void CCCC_Html_Stream::Module_Summary(CCCC_Module *module_ptr)
{
  // calculate the counts on which all displayed data will be based
  // int nof=module_ptr->member_table.records(); // Number of functions
  int nof=0;
  int loc=module_ptr->get_count(COUNT_TAG_LINES_OF_CODE);
  int mvg=module_ptr->get_count(COUNT_TAG_CYCLOMATIC_NUMBER);
  int com=module_ptr->get_count(COUNT_TAG_LINES_OF_COMMENT);

  // the variants of IF4 measure information flow and couplings
  int if4=module_ptr->get_count(COUNT_TAG_INTERMODULE_COMPLEXITY4);
  int if4v=module_ptr->get_count(COUNT_TAG_INTERMODULE_COMPLEXITY4 COUNT_TAG_VISIBLE_SUFFIX);
  int if4c=module_ptr->get_count(COUNT_TAG_INTERMODULE_COMPLEXITY4 COUNT_TAG_CONCRETE_SUFFIX);

  int wmc1=module_ptr->get_count(COUNT_TAG_WEIGHTED_METHODS_PER_CLASS_UNITY);
  int wmcv=module_ptr->get_count(COUNT_TAG_WEIGHTED_METHODS_PER_CLASS COUNT_TAG_VISIBLE_SUFFIX);
  int dit=module_ptr->get_count(COUNT_TAG_INHERITANCE_TREE_DEPTH);
  int noc=module_ptr->get_count(COUNT_TAG_NUMBER_OF_CHILDREN);
  int cbo=module_ptr->get_count(COUNT_TAG_COUPLING_BETWEEN_OBJECTS);

  fstr << HTMLBeginElement(_Table, "summary");

  fstr << HTMLBeginElement(_TableHead)
       << HTMLBeginElement(_TableRow) << endl;
  Put_Header_Cell("Metric",70);
  Put_Header_Cell("Tag",10);
  Put_Header_Cell("Overall",10);
  Put_Header_Cell("Per Function",10);
  fstr << HTMLEndElement(_TableRow)
       << HTMLEndElement(_TableHead) << endl;

  fstr << HTMLBeginElement(_TableRow);
  Put_Label_Cell("Lines of Code");
  Put_Label_Cell("LOC");
  Put_Metric_Cell(loc,"LOCm");
  Put_Metric_Cell(loc,nof,"LOCg");
  fstr << HTMLEndElement(_TableRow);

  fstr << HTMLBeginElement(_TableRow);
  Put_Label_Cell("McCabe's Cyclomatic Number");
  Put_Label_Cell("MVG");
  Put_Metric_Cell(mvg,"MVGm");
  Put_Metric_Cell(mvg,nof,"MVGf");
  fstr << HTMLEndElement(_TableRow);

  fstr << HTMLBeginElement(_TableRow);
  Put_Label_Cell("Lines of Comment");
  Put_Label_Cell("COM");
  Put_Metric_Cell(com,"COMm");
  Put_Metric_Cell(com,nof,"8.3");
  fstr << HTMLEndElement(_TableRow);

  fstr << HTMLBeginElement(_TableRow);
  Put_Label_Cell("LOC/COM");
  Put_Label_Cell("L_C");
  Put_Metric_Cell(loc,com,"L_C");
  fstr << HTMLTableCell("&nbsp;");
  fstr << HTMLEndElement(_TableRow);

  fstr << HTMLBeginElement(_TableRow);
  Put_Label_Cell("MVG/COM");
  Put_Label_Cell("M_C");
  Put_Metric_Cell(mvg,com,"M_C");
  fstr << HTMLTableCell("&nbsp;");
  fstr << HTMLEndElement(_TableRow);

  fstr << HTMLBeginElement(_TableRow);
  Put_Label_Cell("Weighted Methods per Class (weighting = unity)");
  Put_Label_Cell("WMC1");
  Put_Metric_Cell(wmc1);
  fstr << HTMLTableCell("&nbsp;"); // wmc1 should be identical to nof
  fstr << HTMLEndElement(_TableRow);

  fstr << HTMLBeginElement(_TableRow);
  Put_Label_Cell("Weighted Methods per Class (weighting = visible)");
  Put_Label_Cell("WMCv");
  Put_Metric_Cell(wmcv);
  fstr << HTMLTableCell("&nbsp;");
  fstr << HTMLEndElement(_TableRow);

  fstr << HTMLBeginElement(_TableRow);
  Put_Label_Cell("Depth of Inheritance Tree");
  Put_Label_Cell("DIT");
  Put_Metric_Cell(dit);
  fstr << HTMLTableCell("&nbsp;");
  fstr << HTMLEndElement(_TableRow);

  fstr << HTMLBeginElement(_TableRow);
  Put_Label_Cell("Number of Children");
  Put_Label_Cell("NOC");
  Put_Metric_Cell(noc);
  fstr << HTMLTableCell("&nbsp;");
  fstr << HTMLEndElement(_TableRow);

  fstr << HTMLBeginElement(_TableRow);
  Put_Label_Cell("Coupling between objects");
  Put_Label_Cell("CBO");
  Put_Metric_Cell(cbo);
  fstr << HTMLTableCell("&nbsp;");
  fstr << HTMLEndElement(_TableRow);

  fstr << HTMLBeginElement(_TableRow);
  Put_Label_Cell("Information Flow measure (inclusive)");
  Put_Label_Cell("IF4");
  Put_Metric_Cell(if4,1,"IF4");
  Put_Metric_Cell(if4,nof,"8.3");
  fstr << HTMLEndElement(_TableRow);

  fstr << HTMLBeginElement(_TableRow);
  Put_Label_Cell("Information Flow measure (visible)");
  Put_Label_Cell("IF4v");
  Put_Metric_Cell(if4v,1,"IF4v");
  Put_Metric_Cell(if4v,nof,"8.3");
  fstr << HTMLEndElement(_TableRow);

  fstr << HTMLBeginElement(_TableRow);
  Put_Label_Cell("Information Flow measure (concrete)");
  Put_Label_Cell("IF4c");
  Put_Metric_Cell(if4c,1,"IF4c");
  Put_Metric_Cell(if4c,nof,"8.3");
  fstr << HTMLEndElement(_TableRow);

  fstr << HTMLEndElement(_Table);
}


CCCC_Html_Stream::CCCC_Html_Stream(const string& fname, const string& info)
{
  // cerr << "Attempting to open file in directory " << outdir.c_str() << endl;
  fstr.open(fname.c_str());
  if(fstr.good() != TRUE)
    {
      cerr << "failed to open " << fname.c_str()
	   << " for output in directory " << outdir.c_str() << endl;
      exit(1);
    }
  else
    {
      // cerr << "File: " << fname << " Info: " << info << endl;
    }

  string top = _HTMLBoilerplateTop;
  for (size_t s = 0; (s = top.find("${TITLE}", s)) != string::npos; s += info.size())
	  top.replace(s, 8, info);
  fstr << top;
  PopulateJSTooltipMap();
}

int setup_anchor_data()
{
  int i=0;
  Source_Anchor a1("cccc_use.h",12);
  Source_Anchor a2("cccc_htm.h",15);
  i++;
  string key1=a1.key(), key2=a2.key();
  i++;
  source_anchor_map_t::value_type v1(key1,a1);
  source_anchor_map_t::value_type v2(key2,a2);
  i++;
  source_anchor_map.insert(v1);
  source_anchor_map.insert(v2);
  return i;
}


void CCCC_Html_Stream::Source_Listing()
{
  // The variable stream src_str used to be an instance
  // of fstream which gets reopened many times.
  // this worked under Linux but broke under Win32, so
  // this variable is now a pointer which is repeatedly
  // deleted and new'ed

  string current_filename;
  int current_line=0;
  int next_anchor_required=0;
  ifstream *src_str=NULL;
  string style_open = HTMLBeginElement(_Div, "code"), style_close = HTMLEndElement(_Div);

  string filename=outdir;
  filename+="/cccc_src.html";
  CCCC_Html_Stream source_html_str(filename.c_str(),"source file");

  source_anchor_map_t::iterator iter=source_anchor_map.begin();
  while(iter!=source_anchor_map.end())
    {
      char linebuf[1024];
      Source_Anchor& nextAnchor=(*iter).second;
      if(current_filename!=nextAnchor.get_file())
	{
	  current_filename=nextAnchor.get_file();
	  current_line=0;
	  delete src_str;
	  src_str=new ifstream(current_filename.c_str(),std::ios::in);
	  src_str->getline(linebuf,1023);
	  source_html_str.Put_Section_Heading(current_filename.c_str(), current_filename.c_str(), 1);
	}

      while(src_str->good())
	{
	  current_line++;
          source_html_str.fstr << style_open;
	  if(
	     (iter!=source_anchor_map.end()) &&
	     (current_filename==(*iter).second.get_file()) &&
	     (current_line==(*iter).second.get_line())
	     )
	    {
	      (*iter).second.Emit_NAME(source_html_str.fstr);
	      iter++;
	    }
	  else
	    {
	      (*iter).second.Emit_SPACE(source_html_str.fstr);
	    }
 	  source_html_str << linebuf;
 	  source_html_str.fstr << endl;
          source_html_str.fstr << style_close << endl;
	  src_str->getline(linebuf,1023);
 	}

      // if there are any remaining anchors for this file the sorting
      // by line number must be wrong
      // complain and ignore
      while(
	    (iter!=source_anchor_map.end()) &&
	    (current_filename==(*iter).second.get_file())
	    )
	{
          source_html_str.fstr << style_open;
	  (*iter).second.Emit_NAME(source_html_str.fstr);
          source_html_str.fstr << style_close;
	  iter++;
	}
    }

  // delete the last input stream created
  delete src_str;

  source_html_str.fstr << _HTMLBoilerplateBottom;

}

static string pad_string(int target_width, string the_string, string padding)
{
  int spaces_required=target_width-the_string.size();
  string pad_string;
  while(spaces_required>0)
    {
      pad_string+=padding;
      spaces_required--;
    }
  return pad_string+the_string;
}


string Source_Anchor::key() const
{
  char linebuf[16];
  snprintf(linebuf, sizeof(linebuf), "%08d", line_);
  return file_ + ":" + linebuf;
}


void Source_Anchor::Emit_HREF(ofstream& fstr)
{
  string anchor_key=key();

  fstr << "<a class=\"sourceAnchor\" href=\"cccc_src.html#" << anchor_key.c_str() << "\">"
       << file_.c_str() << ":" << line_
       << "</a>";
}

void Source_Anchor::Emit_NAME(ofstream& fstr)
{
  string anchor_key=key();
  char ln_buf[32];
  snprintf(ln_buf, sizeof(ln_buf), "%d", line_);
  string ln_string=pad_string(8, ln_buf, " ");
  string space_string=pad_string(2, ""," ");
  fstr << "<a name=\"" << anchor_key.c_str() << "\"></a>"
       << ln_string.c_str() << space_string.c_str();
}

void Source_Anchor::Emit_SPACE(ofstream& fstr)
{
  string space_string=pad_string(10, "", " ");
  fstr << space_string.c_str();
}

/*****************************************************************************
 * The below calls and constants are dedicated to isolating the raw HTML
 * text from the code proper.
 */
string CCCC_Html_Stream::HTMLSingleEntryRow(int columnCount, const char* txt, int height)
{
    stringstream buf;
    buf << "<tr";
    if (height >= 0)
        buf << " style=\"height: \"" << height << "px;\"";
    buf << "><td colspan=\"" << columnCount << "\">" << txt << "</td></tr>";
    return buf.str();
}

string CCCC_Html_Stream::HTMLMultiColumnHeaderCell(const char* text, int columnCount)
{
    stringstream buf;
    buf << "<th";
    if (columnCount > 1)
        buf << " colspan=\"" << columnCount << "\"";
    buf << ">" << text << "</th>";
    return buf.str();
}

string CCCC_Html_Stream::HTMLBeginElement(const char* nam, const char* clas, int width)
{
    stringstream buf;
    buf << "<" << nam;
    if (clas[0] != 0)
        buf << " class=\"" << clas << "\"";
    if (width >= 0)
        buf << " style=\"width: " << width << "%;\"";
    buf << ">";
    return buf.str();
}

string CCCC_Html_Stream::HTMLTableCell(const char* txt, const char* clasName, int width)
{
    stringstream buf;
    buf << HTMLBeginElement(_TableCell, clasName, width)
	        << txt
	        << HTMLEndElement(_TableCell);
    return buf.str();
}

string CCCC_Html_Stream::HTMLParagraph(const char* txt)
{
    stringstream buf;
    buf << "<p>" << txt << "</p>";
    return buf.str();
}

string CCCC_Html_Stream::HTMLEndElement(const char* nam)
{
    stringstream buf;
    buf << "</" << nam << ">";
    return buf.str();
}

string CCCC_Html_Stream::JSEscapeStringLiteral(const char* inp)
{
    stringstream ret;
    for (const char* p = inp; *p != 0; ++p) {
        switch (*p) {
        case '\'': ret << "\\'"; break;
        case '\\': ret << "\\\\"; break;
        case '\r': break;
        case '\n': ret << "\\n\\\n"; break;
        default: ret << *p; break;
        }
    }
    return ret.str();
}

string CCCC_Html_Stream::HTMLEscapeLiteral(const char* inp)
{
    stringstream buf;
    for (const char* cptr = inp; *cptr != 0; ++cptr) {
        // the purpose of this is to filter out the characters which
        // must be escaped in HTML
        switch(*cptr) {
        case '>': buf << "&gt;" ; break;
        case '<': buf << "&lt;" ; break;
        case '&': buf << "&amp;"; break;
        // commas and parentheses do not need to be escaped, but
        // we want to allow line breaking just inside
        // parameter lists and after commas
        // we insert a non-breaking space to guarantee a small indent
        // on the new line, and one before the right parenthesis for
        // symmetry
        case ',': buf << ", &nbsp;" ; break;
        case '(': buf << "( &nbsp;" ; break;
        case ')': buf << "&nbsp;)" ; break;
        default : buf << (*cptr);
        }
    }
    return buf.str();
}

void CCCC_Html_Stream::PopulateJSTooltipMap()
{
    const metric_description_t* ptrs[] = { &ProjectSummaryMetrics[0], &OODesignMetrics[0], &StructuralSummaryMetrics[0] };
    int counts[] = { COUNTOF(ProjectSummaryMetrics), COUNTOF(OODesignMetrics), COUNTOF(StructuralSummaryMetrics) };
    fstr << "<script type=\"text/javascript\">";
    for (int k = 0; k < COUNTOF(ptrs); ++k) {
        for (int j = 0; j < counts[k]; ++j)
            fstr << "  window.g_Glossary['"
                 << JSEscapeStringLiteral(ptrs[k][j].abbreviation) << "'] = {"
                    "  name: '" << JSEscapeStringLiteral((strlen(ptrs[k][j].name) == 0) ? ptrs[k][j].abbreviation : ptrs[k][j].name) << "', "
                    "  description: '" << JSEscapeStringLiteral(ptrs[k][j].description) << "' };\n";
    }
    fstr << "</script>" << endl;
}

const char* CCCC_Html_Stream::_Table = "table";
const char* CCCC_Html_Stream::_TableHead = "thead";
const char* CCCC_Html_Stream::_TableFoot = "tfoot";
const char* CCCC_Html_Stream::_TableRow = "tr";
const char* CCCC_Html_Stream::_TableHeader = "th";
const char* CCCC_Html_Stream::_TableCell = "td";
const char* CCCC_Html_Stream::_Anchor = "a";
const char* CCCC_Html_Stream::_ListItem = "li";
const char* CCCC_Html_Stream::_Span = "span";
const char* CCCC_Html_Stream::_Div = "div";
const char* CCCC_Html_Stream::_UnorderedList = "ul";
const char* CCCC_Html_Stream::_HTMLLineBreak = "<br />";

const char* CCCC_Html_Stream::_HTMLBoilerplateTop =
        "<!DOCTYPE html>\n"
        "<html>\n"
        "  <head>\n"
        "    <title>${TITLE}</title>\n"
        "  </head>\n"
        "  <style type=\"text/css\">\n"
        "      body {\n"
        "          font-family: sans-serif;\n"
        "          margin-left: 8pt;\n"
        "          margin-right: 8pt;\n"
        "          width: 98%;\n"
        "          position: relative;\n"
        "      }\n"
        "      p {\n"
        "          margin-top: 8pt;\n"
        "      }\n"
        "      table {\n"
        "           width: 100%;\n"
        "           border-top: 1px solid black;\n"
        "           border-spacing: 0;\n"
        "           border-collapse: collapse;\n"
        "           border-left: 1px solid black;\n"
        "      }\n"
        "      table.toc th {\n"
        "           background-color: aqua;\n"
        "           padding: 3pt;\n"
        "      }\n"
        "      table.toc td {\n"
        "           padding: 2pt;\n"
        "      }\n"
        "      table td, table th {\n"
        "           border-bottom: 1px solid black;\n"
        "           border-right: 1px solid black;\n"
        "      }\n"
        "      td.toc_entry_name {\n"
        "           text-align: left;\n"
        "      }\n"
        "      table.summary th {\n"
        "           background-color: aqua;\n"
        "      }\n"
        "      table.summary td {\n"
        "           font-family: monospace;\n"
        "      }\n"
        "      *.code {\n"
        "           font-family: monospace;\n"
        "           white-space: pre;\n"
        "      }\n"
        "      span.metric_low {\n"
        "      }\n"
        "      span.metric_medium {\n"
        "           font-style: italic;\n"
        "      }\n"
        "      span.metric_high {\n"
        "           font-weight: bold;\n"
        "      }\n"
        "      td.metric_low, td.metric_medium, td.metric_high {\n"
        "           text-align: right;\n"
        "      }\n"
        "      td.metric_low {\n"
        "      }\n"
        "      td.metric_medium {\n"
        "          background-color: yellow;\n"
        "      }\n"
        "      td.metric_high {\n"
        "          background-color: red;\n"
        "      }\n"
        "      a {\n"
        "          text-decoration: none;\n"
        "      }\n"
        "      a:visited {\n"
        "          color: purple;\n"
        "      }\n"
        "      a:hover {\n"
        "          color: red;\n"
        "      }\n"
        "      *.label {\n"
        "          background-color: #eee;\n"
        "      }\n"
        "      li p {\n"
        "          margin-top: 2pt;\n"
        "          margin-bottom: 4pt;\n"
        "          padding-left: 10pt;\n"
        "      }\n"
        "      div.tooltip {\n"
        "          display: none;\n"
        "          position: fixed;\n"
        "          border: 1px solid black;\n"
        "          background-color: #ddffdd;\n"
        "          width: 300px;\n"
        "          z-index: 2;\n"
        "          font-size: smaller;\n"
        "          padding: 5pt;\n"
        "          text-align: justify;\n"
        "      }\n"
        "      div.stickyHead {\n"
        "         display: none;\n"
        "         width: 98%;\n"
        "         position: fixed;\n"
        "         top: 0pt;\n"
        "         z-index: 1;\n"
        "      }\n"
        "      div.ttname {\n"
        "          font-weight: bold;\n"
        "          margin-bottom: 5pt;\n"
        "      }\n"
        "      #debug {\n"
        "          border: 1px solid black;\n"
        "          background-color: red;\n"
        "          position: fixed;\n"
        "          top: 100pt;\n"
        "          padding: 8pt;\n"
        "          width: 80%;\n"
        "          display: none;\n"
        "      }\n"
        "  </style>\n"
        "  <body>\n"
        "  <div id=\"debug\"></div>\n"
        "  <script type=\"text/javascript\">\n"
        "     window.g_Glossary = {}; // list of objects with name, description attributes for use in tooltips\n"
        "\n"
        "     function GetText(elem) {\n"
        "         var ret = \"\";\n"
        "         for (var cld = elem.firstChild; cld != null; cld = cld.nextSibling) {\n"
        "             switch (cld.nodeType) {\n"
        "             case 1: ret += GetText(cld); break;\n"
        "             case 3: ret += cld.nodeValue; break;\n"
        "             }\n"
        "         }\n"
        "         return ret.trim();\n"
        "     }\n"
        "     function GetElementPosition(elem) {\n"
        "         // return element position in page coords\n"
        "         var ret = { left: 0, top: 0 };\n"
        "         while (elem != null && elem != undefined) {\n"
        "             ret.left += elem.offsetLeft;\n"
        "             ret.top  += elem.offsetTop;\n"
        "             elem = elem.offsetParent;\n"
        "         }\n"
        "         return ret;\n"
        "     }\n"
        "     function SetupTooltip(elem, description) {\n"
        "         var div = document.createElement(\"div\");\n"
        "         div.className = \"tooltip\";\n"
        "         div.innerHTML = \"<div class=\\\"ttname\\\">\" + description.name + \"</div>\\n<div class=\\\"ttdescr\\\">\" + description.description + \"</div>\";\n"
        "         document.body.appendChild(div);\n"
        "         elem.addEventListener(\"mousemove\", function(ev) {\n"
        "             div.style.display = \"block\";\n"
        "             div.style.top = (ev.clientY + 5) + \"px\";\n"
        "             div.style.left = (ev.clientX - 305) + \"px\";\n"
        "         });\n"
        "         elem.addEventListener(\"mouseout\", function(ev) {\n"
        "             div.style.display = \"none\";\n"
        "         });\n"
        "     }\n"
        "     // Expect a th here. Assign a callback to it on-click such that it sorts all the rows in the table body\n"
        "     // according to the th.cellIndex'th element of the row.\n"
        "     function GetParentWithName(elem, name) {\n"
        "         var ret = elem;\n"
        "         while (ret != null && ret.nodeName.match(name) == null)\n"
        "             ret = ret.parentNode;\n"
        "         return ret;\n"
        "     }\n"
        "     function GetTableRows(elemInTable) {\n"
        "         var ret = [];\n"
        "         var tbody = GetParentWithName(elemInTable, /table/i);\n"
        "         // Sticky head support: Use original table if present\n"
        "         if (tbody.Original)\n"
        "             tbody = tbody.Original;\n"
        "         for (tbody = tbody.firstChild; tbody != null && tbody.nodeName.match(/tbody/i) == null; tbody = tbody.nextSibling);\n"
        "         for (var tr = tbody.firstChild; tr != null; tr = tr.nextSibling) {\n"
        "             if (tr.nodeName.match(/tr/i))\n"
        "                 ret.push(tr);\n"
        "         }\n"
        "         return { 'tbody': tbody, 'rows': ret };\n"
        "     }\n"
        "     function GetColumn(elem) {\n"
        "         /* FIXME I think you can still foil this with some pathlogical layouts - like staggering\n"
        "            multi-row squares in a stair-step pattern. Not intending to do that today, though. */\n"
        "         var column = 0, rowInd = 1;\n"
        "         for (var cell = elem.previousSibling; cell != null; cell = cell.previousSibling) {\n"
        "             if (cell.nodeType == 1)\n"
        "                 column += cell.colSpan;\n"
        "         }\n"
        "         for (var row = elem.parentNode.previousSibling; row != null; row = row.previousSibling) {\n"
        "             if (row.nodeType != 1)\n"
        "                 continue;\n"
        "             var colInd = 0, insertCols = 0;\n"
        "             for (var cell = row.firstChild; cell != null && colInd <= column; cell = cell.nextSibling) {\n"
        "                 if (cell.nodeType != 1)\n"
        "                     continue;\n"
        "                 if (cell != elem && cell.rowSpan - rowInd > 0 /* this element contributes to this column offset */)\n"
        "                     insertCols += cell.colSpan;\n"
        "                 colInd += cell.colSpan;\n"
        "             }\n"
        "             column += insertCols;\n"
        "             rowInd += 1;\n"
        "         }\n"
        "         return column;\n"
        "     }\n"
        "     function SetupSortColumn(elem) {\n"
        "         elem.firstChild.addEventListener(\"click\", function(ev) {\n"
        "             var z = GetTableRows(elem);\n"
        "             var column = GetColumn(elem);\n"
        "             elem.SortDescending = (elem.SortDescending ? false : true);\n"
        "             z.rows.sort(function(a, b) {\n"
        "                 var ret = 0;\n"
        "                 var a_text = GetText(a.cells[column]);\n"
        "                 var b_text = GetText(b.cells[column]);\n"
        "                 var a_num = parseFloat(a_text);\n"
        "                 var b_num = parseFloat(b_text);\n"
        "                 if (isNaN(a_num) && isNaN(b_num))\n"
        "                     ret = (a_text > b_text) ?  1 :\n"
        "                           (a_text < b_text) ? -1 :\n"
        "                           (a_text >= b_text) ? 1 :\n"
        "                           -1;\n"
        "                 else if (isNaN(a_num))\n"
        "                     ret = -1;\n"
        "                 else if (isNaN(b_num))\n"
        "                     ret =  1;\n"
        "                 else\n"
        "                     ret = a_num - b_num;\n"
        "                 // tiebreakers\n"
        "                 return elem.SortDescending ? -ret : ret;\n"
        "             });\n"
        "             for (var k = 0; k < z.rows.length; ++k)\n"
        "                 z.tbody.removeChild(z.rows[k]);\n"
        "             for (var k = 0; k < z.rows.length; ++k)\n"
        "                 z.tbody.appendChild(z.rows[k]);\n"
        "         });\n"
        "     }\n"
        "     function GetScrollOffset() {\n"
        "         return (!isNaN(window.pageYOffset)) ? { 'X': window.pageXOffset,       'Y': window.pageYOffset } :\n"
        "                                               { 'X': document.body.scrollLeft, 'Y': document.body.scrollTop };\n"
        "     }\n"
        "     function StickyHead(table, thead) {\n"
        "         if (table.Original)\n"
        "             return;\n"
        "         var dupl = thead.cloneNode(true);\n"
        "         var div = document.createElement(\"div\");\n"
        "         div.className = \"stickyHead\";\n"
        "         var newTable = table.cloneNode(false);\n"
        "         newTable.Original = table;\n"
        "         div.appendChild(newTable);\n"
        "         newTable.appendChild(dupl);\n"
        "         document.body.appendChild(div);\n"
        "         window.addEventListener(\"scroll\", function() {\n"
        "             var headPos = GetElementPosition(thead);\n"
        "             var bodyPos = GetElementPosition(table);\n"
        "             var scrolly = GetScrollOffset().Y;\n"
        "             if (headPos.top < scrolly && bodyPos.top + table.scrollHeight > scrolly) {\n"
        "                 div.style.display = \"block\";\n"
        "             } else {\n"
        "                 div.style.display = \"none\";\n"
        "             }\n"
        "         });\n"
        "     }\n"
        // Add tooltips from the glossary, sort capabilities
        "     function EnhanceTable(table) {\n"
        "         var headerNodes = table.getElementsByTagName(\"th\");\n"
        "         for(var k = 0; k < headerNodes.length; ++k) {\n"
        "             var th = headerNodes[k];\n"
        "             var possibleAbbr = GetText(th);\n"
        "             if (th.className.indexOf(\"sortable\") != -1)\n"
        "                 SetupSortColumn(th, th);\n"
        "             if (possibleAbbr in window.g_Glossary)\n"
        "                 SetupTooltip(th, window.g_Glossary[possibleAbbr]);\n"
        "         }\n"
        // Stickify any theads
        "         headerNodes = table.getElementsByTagName(\"thead\");\n"
        "         for (var k = 0; k < headerNodes.length; ++k) {\n"
        "             var thead = headerNodes[k];\n"
        "             StickyHead(table, thead);\n"
        "         }\n"
        "     }\n"
        "\n"
        "     window.addEventListener(\"load\", function() {\n"
        "         var tables = document.getElementsByTagName(\"table\");\n"
        "         for (var k = 0; k < tables.length; ++k) {\n"
        "             var table = tables[k];\n"
        "             if (table.className.indexOf(\"summary\") != -1)\n"
        "                 EnhanceTable(table);\n"
        "         }\n"
        "     });\n"
        "  </script>\n";

const char* CCCC_Html_Stream::_HTMLBoilerplateBottom =
        "  </body>\n"
        "</html>";

#ifdef UNIT_TEST
int main()
{
  CCCC_Project *prj_ptr=test_project_ptr();
  CCCC_Html_Stream os(*prj_ptr,"cccc.htm",".");
  return 0;
}
#endif
