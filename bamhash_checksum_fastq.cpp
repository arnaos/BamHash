#include <iostream>
#include <seqan/stream.h>
#include <seqan/seq_io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <cmath>
#include <cstdlib>
#include <stdint.h>
#include <vector>
#include <seqan/arg_parse.h>

#include "bamhash_checksum_common.h"

struct Fastqinfo {
  std::vector<std::string> fastqfiles;
  bool debug;
  bool noReadNames;
  bool noQuality;
  bool paired;

  Fastqinfo() : debug(false), noReadNames(false), noQuality(false), paired(true) {}

};

seqan::ArgumentParser::ParseResult
parseCommandLine(Fastqinfo& options, int argc, char const **argv) {
  // Setup ArgumentParser.
  seqan::ArgumentParser parser("bamhash_checksum_fastq");
  //readlink("/proc/self/exe", options.bindir, sizeof(options.bindir)-1);

  setShortDescription(parser, "Checksum of a set of fastq files");
  setVersion(parser, BAMHASH_VERSION);
  setDate(parser, "May 2015");

  addUsageLine(parser, "[\\fIOPTIONS\\fP] \\fI<in1.fastq.gz>\\fP [\\fIin2.fastq.gz ... \\fP]");
  addDescription(parser, "Program for checksum of sequence reads. ");

  addArgument(parser, seqan::ArgParseArgument(seqan::ArgParseArgument::INPUTFILE,"fastqfiles", true));

  setValidValues(parser, 0,"fq fq.gz fastq fastq.gz");

  addSection(parser, "Options");
  //add debug option:
  addOption(parser, seqan::ArgParseOption("d", "debug", "Debug mode. Prints full hex for each read to stdout"));
  addOption(parser, seqan::ArgParseOption("R", "no-readnames", "Do not use read names as part of checksum"));
  addOption(parser, seqan::ArgParseOption("Q", "no-quality", "Do not use read quality as part of checksum"));
  addOption(parser, seqan::ArgParseOption("P", "no-paired", "List of fastq files are not paired-end reads"));

  // Parse command line.
  seqan::ArgumentParser::ParseResult res = seqan::parse(parser, argc, argv);
  if (res != seqan::ArgumentParser::PARSE_OK) {
    return res;
  }

  options.debug = seqan::isSet(parser, "debug");
  options.noReadNames = seqan::isSet(parser, "no-readnames");
  options.noQuality = seqan::isSet(parser, "no-quality");
  options.paired = !seqan::isSet(parser, "no-paired");

  options.fastqfiles = getArgumentValues(parser, 0);

  
  return seqan::ArgumentParser::PARSE_OK;
}


int main(int argc, char const **argv) {
  Fastqinfo info; // Define structure variable
  seqan::ArgumentParser::ParseResult res = parseCommandLine(info, argc, argv); // Parse the command line.

  if (res != seqan::ArgumentParser::PARSE_OK) {
    return res == seqan::ArgumentParser::PARSE_ERROR;
  }

  // Define:
  uint64_t sum = 0;
  unsigned count = 0;
  seqan::StringSet<seqan::CharString> idSub1;
  seqan::StringSet<seqan::CharString> idSub2;
  seqan::CharString string2hash1;
  seqan::CharString string2hash2;
  seqan::CharString id1;
  seqan::CharString id2;
  seqan::CharString seq1;
  seqan::CharString seq2;
  seqan::CharString qual1;
  seqan::CharString qual2;
  hash_t hex1;
  hash_t hex2;

  // Open GZStream
  seqan::Stream<seqan::GZFile> gzStream1;
  seqan::Stream<seqan::GZFile> gzStream2;


  if (info.paired && (info.fastqfiles.size() % 2 != 0)) {
    std::cerr << "ERROR: Running with paired end mode, but supplied an odd number of input files ";
    for (int i = 0; i < info.fastqfiles.size(); i++) {
      std::cerr << info.fastqfiles[i] << " ";
    }
    std::cerr << std::endl;
    return 1;
  }

  for (int i = 0; i < info.fastqfiles.size(); i += (info.paired) ? 2 : 1) {
    const char* fastq1 = info.fastqfiles[i].c_str();
    const char* fastq2 = "";
    if (info.paired) {
     fastq2 = info.fastqfiles[i+1].c_str();
    }
    
    if (!open(gzStream1, fastq1, "r")) {
      std::cerr << "ERROR: Could not open the file: " << fastq1 << " for reading.\n";
      return 1;
    }

    if (info.paired && !open(gzStream2, fastq2, "r")) {
      std::cerr << "ERROR: Could not open the file: " << fastq2 << " for reading.\n";
      return 1;
    }

    //Setup RecordReader for reading FASTQ file from gzip-compressed file
    seqan::RecordReader<seqan::Stream<seqan::GZFile>, seqan::SinglePass<> > reader1(gzStream1);
    seqan::RecordReader<seqan::Stream<seqan::GZFile>, seqan::SinglePass<> > reader2(gzStream2);


    // Read record
    while (!atEnd(reader1)) {
      if(info.paired) {
        if(atEnd(reader2)) { break; }
      }
      if (readRecord(id1, seq1, qual1, reader1, seqan::Fastq()) != 0) {
        if (atEnd(reader1)) {
          std::cerr << "WARNING: Could not continue reading " << fastq1 <<  " at line: " << count+1 << ".\n";
          return 1;
        }
        std::cerr << "ERROR: Could not read from " << fastq1 << "\n";
        return 1;
      }

      if (info.paired && readRecord(id2, seq2, qual2, reader2, seqan::Fastq()) != 0) {
        if (atEnd(reader2)) {
          std::cerr << "WARNING: Could not continue reading " << fastq2 << " at line: " << count+1 << ". Check if files have the same number of reads.\n";
          return 1;
        }
        std::cerr << "ERROR: Could not read from " << fastq2 << "\n";
        return 1;
      }

      count +=1;


      // If include id, then cut id on first whitespace
      if (seqan::endsWith(id1,"/1") || seqan::endsWith(id1,"/2")) {
        seqan::strSplit(idSub1, id1, '/', false, 1);
      } else {
        seqan::strSplit(idSub1, id1, ' ', false, 1);
      }

      if (info.paired) {
        if (seqan::endsWith(id2,"/1") || seqan::endsWith(id2,"/2")) {
          seqan::strSplit(idSub2, id2, '/', false, 1);
        } else {
          seqan::strSplit(idSub2, id2, ' ', false, 1);
        }
      }

      // Check if names are in same order in both files
      if (info.paired && !info.noReadNames && !(idSub1[0] ==  idSub2[0])) {
        std::cerr << "WARNING: Id_names in line: " << count << " are not in the same order\n";
        return 1;
      }

      if (!info.noReadNames) {
        seqan::append(string2hash1, idSub1[0]);
        seqan::append(string2hash1,"/1");
      }
      seqan::append(string2hash1, seq1);
      if (!info.noQuality) {
        seqan::append(string2hash1, qual1);
      }


      if (info.paired) {
        if (!info.noReadNames) {
          seqan::append(string2hash2, idSub2[0]);
          seqan::append(string2hash2,"/2");
        }
        seqan::append(string2hash2, seq2);
        if (!info.noQuality) {
          seqan::append(string2hash2, qual2);
        }
      }

      // Get MD5 hash
      hex1 = str2md5(toCString(string2hash1), length(string2hash1));
      if(info.paired) { hex2 = str2md5(toCString(string2hash2), length(string2hash2)); }

      if (info.debug) {
        std::cout << string2hash1 << " " <<   std::hex << hex1.p.low << "\n";
        if(info.paired) { std::cout << string2hash2 << std::hex << hex2.p.low << "\n"; }
      } else {
        hexSum(hex1, sum);
        if(info.paired) { hexSum(hex2, sum); }
      }

      seqan::clear(string2hash1);
      seqan::clear(string2hash2);
      seqan::clear(idSub1);
      seqan::clear(idSub2);

    }

  }

  if (!info.debug) {
    std::cout << std::hex << sum << "\t";
    std::cout << std::dec << count << "\n";
  }

    
  return 0;
}

