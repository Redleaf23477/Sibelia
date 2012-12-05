//***************************************************************************
//* Copyright (c) 2012 Saint-Petersburg Academic University
//* All Rights Reserved
//* See file LICENSE for details.
//****************************************************************************

#include "outputgenerator.h"
#include "platform.h"

namespace SyntenyFinder
{
	namespace
	{
		const char COVERED = 1;
		typedef std::pair<size_t, std::vector<BlockInstance> > GroupedBlock;
		typedef std::vector<GroupedBlock> GroupedBlockList;

		bool ByFirstElement(const GroupedBlock & a, const GroupedBlock & b)
		{
			return a.first < b.first;
		}

		std::string OutputIndex(const BlockInstance & block)
		{
			std::stringstream out;
			out << block.GetChrInstance().GetConventionalId() << '\t' << (block.GetSignedBlockId() < 0 ? '-' : '+') << '\t';
			out << block.GetConventionalStart() << '\t' << block.GetConventionalEnd() << '\t' << block.GetEnd() - block.GetStart();
			return out.str();
		}

		// function to sort blocks in one chromosome by starting position (it's sorted lexicographically by D3)
		std::string OutputD3BlockID(const BlockInstance & block)
		{
			std::stringstream out;
			// label should (1) have no dots, and (2) be valid DOM id/class identifier
			// (exception: it may contains spaces, I'll handle it in javascript later)
//			std::string label = block.GetChrInstance().GetDescription();
//			std::replace(label.begin(), label.end(), '|', ' ');
//			std::replace(label.begin(), label.end(), ':', ' ');
//			std::replace(label.begin(), label.end(), '.', ' ');
			out << "seq" << block.GetChrInstance().GetConventionalId() << ".";
			out << "seq " << block.GetChrInstance().GetConventionalId() << " - ";
			out << std::setfill(' ') << std::setw(8) << block.GetConventionalStart() << " - ";
			out << std::setfill(' ') << std::setw(8) << block.GetConventionalEnd();
			return out.str();
		}

		void OutputLink(std::vector<BlockInstance>::iterator block, int color, int fillLength,
						int linkId, std::ostream& stream)
		{
			int start = block->GetConventionalStart();
			int end = block->GetConventionalEnd();
			if (start > end) std::swap(start, end);

			stream << "block_" << std::setw(fillLength) << std::setfill('0') << linkId << " ";
			stream << "seq" << block->GetChrId() + 1 << " ";
			stream << start << " " << end;
			stream << " color=chr" << color << "_a2" << std::endl;
		}

		template<class Iterator>
			void OutputLines(Iterator start, size_t length, std::ostream & out)
			{
				for(size_t i = 1; i <= length; i++, ++start)
				{
					out << *start;
					if(i % 80 == 0 && i != length)
					{
						out << std::endl;
					}
				}
			}

		std::vector<double> CalculateCoverage(const ChrList & chrList, GroupedBlockList::const_iterator start, GroupedBlockList::const_iterator end)
		{
			std::vector<double> ret;
			std::vector<char> cover;
			double totalBp = 0;
			double totalCoveredBp = 0;
			for(size_t chr = 0; chr < chrList.size(); chr++)
			{
				totalBp += chrList[chr].GetSequence().size();
				cover.assign(chrList[chr].GetSequence().size(), 0);
				for(GroupedBlockList::const_iterator it = start; it != end; ++it)
				{
					for(size_t i = 0; i < it->second.size(); i++)
					{
						if(it->second[i].GetChrInstance().GetId() == chr)
						{
							std::fill(cover.begin() + it->second[i].GetStart(), cover.begin() + it->second[i].GetEnd(), COVERED);
						}
					}
				}

				double nowCoveredBp = static_cast<double>(std::count(cover.begin(), cover.end(), COVERED));
				ret.push_back(nowCoveredBp / cover.size() * 100);
				totalCoveredBp += nowCoveredBp;
			}

			ret.insert(ret.begin(), totalCoveredBp / totalBp * 100);
			return ret;
		}
	}

	void OutputGenerator::ListChrs(std::ostream & out) const
	{
		out << "Seq_id\tSize\tDescription" << std::endl;
		for(size_t i = 0; i < chrList_.size(); i++)
		{
			out << i + 1 << '\t' << chrList_[i].GetSequence().size() << '\t' << chrList_[i].GetDescription() << std::endl;
		}

		out << DELIMITER << std::endl;
	}

	void OutputGenerator::GenerateReport(const std::string & fileName) const
	{
		std::ofstream out;
		TryOpenFile(fileName, out);
		GroupedBlockList sepBlock;
		std::vector<IndexPair> group;
		GroupBy(blockList_, compareById, std::back_inserter(group));
		for(std::vector<IndexPair>::iterator it = group.begin(); it != group.end(); ++it)
		{
			sepBlock.push_back(std::make_pair(it->second - it->first, std::vector<BlockInstance>(blockList_.begin() + it->first, blockList_.begin() + it->second)));
		}

		ListChrs(out);
		out << "Degree\tCount\tTotal";
		for(size_t i = 0; i < chrList_.size(); i++)
		{
			out << "\tSeq " << i + 1;
		}

		out << std::endl;
		group.clear();
		GroupBy(sepBlock, ByFirstElement, std::back_inserter(group));
		group.push_back(IndexPair(0, sepBlock.size()));
		for(std::vector<IndexPair>::iterator it = group.begin(); it != group.end(); ++it)
		{
			if(it != group.end() - 1)
			{
				out << sepBlock[it->first].first << '\t' << it->second - it->first << '\t';
			}
			else
			{
				out << "All\t" << it->second - it->first << "\t";
			}

			out.precision(2);
			out.setf(std::ostream::fixed);
			std::vector<double> coverage = CalculateCoverage(chrList_, sepBlock.begin() + it->first, sepBlock.begin() + it->second);
			std::copy(coverage.begin(), coverage.end(), std::ostream_iterator<double>(out, "%\t"));
			out << std::endl;
		}

		out << DELIMITER << std::endl;
	}

	void OutputGenerator::ListChromosomesAsPermutations(const std::string & fileName) const
	{
		std::ofstream out;
		TryOpenFile(fileName, out);
		std::vector<IndexPair> group;
		GroupBy(blockList_, compareByChrId, std::back_inserter(group));
 		for(std::vector<IndexPair>::iterator it = group.begin(); it != group.end(); ++it)
		{
			out.setf(std::ios_base::showpos);
			size_t length = it->second - it->first;
			size_t chr = blockList_[it->first].GetChrInstance().GetId();
			out << '>' << chrList_[chr].GetDescription() << std::endl;
			std::sort(blockList_.begin() + it->first, blockList_.begin() + it->second);
			CopyN(CFancyIterator(blockList_.begin() + it->first, boost::bind(&BlockInstance::GetSignedBlockId, _1), 0), length, std::ostream_iterator<int>(out, " "));
			out << "$" << std::endl;
		}
	}

	void OutputGenerator::ListBlocksIndices(const std::string & fileName) const
	{
		std::ofstream out;
		TryOpenFile(fileName, out);
		ListChrs(out);
		std::vector<IndexPair> group;
		GroupBy(blockList_, compareById, std::back_inserter(group));
		for(std::vector<IndexPair>::iterator it = group.begin(); it != group.end(); ++it)
		{
			size_t length = it->second - it->first;
			std::sort(blockList_.begin() + it->first, blockList_.begin() + it->second, compareByChrId);
			out << "Block #" << blockList_[it->first].GetBlockId() << std::endl;
			out << "Seq_id\tStrand\tStart\tEnd\tLength" << std::endl;
			CopyN(CFancyIterator(blockList_.begin() + it->first, OutputIndex, std::string()), length, std::ostream_iterator<std::string>(out, "\n"));
			out << DELIMITER << std::endl;
		}
	}

	void OutputGenerator::ListBlocksSequences(const std::string & fileName) const
	{
		std::ofstream out;
		TryOpenFile(fileName, out);
		std::vector<IndexPair> group;
		GroupBy(blockList_, compareById, std::back_inserter(group));
		for(std::vector<IndexPair>::iterator it = group.begin(); it != group.end(); ++it)
		{
			for(size_t block = it->first; block < it->second; block++)
			{
				size_t length = blockList_[block].GetLength();
				char strand = blockList_[block].GetSignedBlockId() > 0 ? '+' : '-';
				const FASTARecord & chr = blockList_[block].GetChrInstance();
				out << ">Seq=\"" << chr.GetDescription() << "\",Strand='" << strand << "',";
				out << "Block_id=" << blockList_[block].GetBlockId() << ",Start=" ;
				out << blockList_[block].GetConventionalStart() << ",End=" << blockList_[block].GetConventionalEnd() << std::endl;

				if(blockList_[block].GetSignedBlockId() > 0)
				{
					OutputLines(chr.GetSequence().begin() + blockList_[block].GetStart(), length, out);
				}
				else
				{
					std::string::const_reverse_iterator it(chr.GetSequence().begin() + blockList_[block].GetEnd());
					OutputLines(CFancyIterator(it, DNASequence::Translate, ' '), length, out);
				}

				out << std::endl;
			}
		}
	}

	void OutputGenerator::GenerateCircosOutput(const std::string & outFile, const std::string & outDir) const
	{
		const int MAX_COLOR = 25;
		//copy template file
		CreateDirectory(outDir);
		std::ofstream out;
		TryOpenFile(outFile, out);
		out << circosTemplate;

		//blocks must be sorted by id
		BlockList sortedBlocks = blockList_;
		std::sort(sortedBlocks.begin(), sortedBlocks.end(), compareById);

		//write link and highlights file
		int idLength = static_cast<int>(log10(static_cast<double>(sortedBlocks.size()))) + 1;
		int lastId = 0;
		int linkCount = 0;
		BlockList blocksToLink;
		std::ofstream linksFile;
		std::ofstream highlightFile;
		TryOpenFile(outDir + "/circos.segdup.txt", linksFile);
		TryOpenFile(outDir + "/circos.highlight.txt", highlightFile);

		int color = 0;
		for(BlockList::iterator itBlock = sortedBlocks.begin(); itBlock != sortedBlocks.end(); ++itBlock)
		{
			highlightFile << "seq" << itBlock->GetChrInstance().GetConventionalId() << " ";
			int blockStart = itBlock->GetConventionalStart();
			int blockEnd = itBlock->GetConventionalEnd();
			if (blockStart > blockEnd)
			{
				std::swap(blockStart, blockEnd);
			}
			highlightFile << blockStart << " " << blockEnd << std::endl;

			if (itBlock->GetBlockId() != lastId)
			{
				blocksToLink.clear();
				lastId = itBlock->GetBlockId();
			}
			for (BlockList::iterator itPair = blocksToLink.begin(); itPair != blocksToLink.end(); ++itPair)
			{
				color = (color + 1) % MAX_COLOR;
				//link start
				OutputLink(itBlock, color, idLength, linkCount, linksFile);
				//link end
				OutputLink(itPair, color, idLength, linkCount, linksFile);
				++linkCount;
			}
			blocksToLink.push_back(*itBlock);
		}

		//write kariotype file
		std::ofstream karFile;
		TryOpenFile(outDir + "/circos.sequences.txt", karFile);

		for (size_t i = 0; i < chrList_.size(); ++i)
		{
			int colorId = (i + 1) % MAX_COLOR;
			karFile << "chr - seq" << i + 1 << " " << chrList_[i].GetDescription() << " 0 " << chrList_[i].GetSequence().length();
			karFile	<< " chr" << colorId << std::endl;
		}
	}

	void OutputGenerator::GenerateD3Output(const std::string & outFile) const
	{
		std::istringstream htmlTemplate(d3Template);

        //open output file
        std::ofstream out;
        TryOpenFile(outFile, out);

		std::string buffer;
		for(;;)
		{
			std::getline(htmlTemplate, buffer);
			if (buffer != "//SIBELIA_MARK_INSERT")
			{
				out << buffer << std::endl;
			}
			else
			{
				break;
			}
		}

        out << "chart_data = [" << std::endl;

        //blocks must be sorted by start
        BlockList sortedBlocks = blockList_;
        std::sort(sortedBlocks.begin(), sortedBlocks.end(), compareByStart);

        // write to output file
        int lastId = 0;
        bool first_line = true;
		for(BlockList::iterator itBlock = sortedBlocks.begin(); itBlock != sortedBlocks.end(); ++itBlock) // O(N^2) by number of blocks, can be optimized
		{
            if (!first_line)
                out << ",";
            else
                first_line = false;
            out << "    {";
            out << "\"name\":\"" << OutputD3BlockID(*itBlock) << "\",";
            out << "\"size\":" << itBlock->GetLength() << ",";
            out << "\"imports\":[";
            bool first = true;
            for (BlockList::iterator itPair = sortedBlocks.begin(); itPair != sortedBlocks.end(); ++itPair)
            {
                if (itPair->GetBlockId() == itBlock->GetBlockId() && itPair != itBlock)
                {
                    if (!first)
                        out << ",";
                    else
                        first = false;
                    out << "\"" << OutputD3BlockID(*itPair) << "\"";
                }
			}
            out << "]";
            out << "}" << std::endl;
		}
        out << "];" << std::endl;

        // making data for chart legend
        out << "chart_legend = [" << std::endl;
        first_line = true;
        for(size_t i = 0; i < chrList_.size(); i++)
        {
            if (!first_line)
                out << ",";
            else
                first_line = false;
           out << "    \"seq " << chrList_[i].GetId() + 1 << " : " <<  chrList_[i].GetDescription() << "\"" << std::endl;
        }
        out << "];" << std::endl;

        //write rest of html template
		while (!htmlTemplate.eof())
		{
			std::getline(htmlTemplate, buffer);
			out << buffer << std::endl;
		}
	}

	void OutputGenerator::TryOpenFile(const std::string & fileName, std::ofstream & stream) const
	{
		stream.open(fileName.c_str());
		if(!stream)
		{
			throw std::runtime_error(("Cannot open file " + fileName).c_str());
		}
	}

	void OutputGenerator::TryOpenResourceFile(const std::string & fileName, std::ifstream & stream) const
	{
		std::vector<std::string> dirs = GetResourceDirs();
		for (std::vector<std::string>::iterator itDirs = dirs.begin(); itDirs != dirs.end(); ++itDirs)
		{
			stream.open((*itDirs + "/" + fileName).c_str());
			if (stream) break;
		}
		if (!stream)
		{
			throw std::runtime_error(("Cannot find resource file: " + fileName).c_str());
		}
	}

	void OutputGenerator::OutputBuffer(const std::string & fileName, const std::string & buffer) const
	{
		std::ofstream out;
		TryOpenFile(fileName, out);
		out << buffer;
	}

	void GlueBlock(const std::string block[], size_t blockSize, std::string & buf)
	{
		std::stringstream ss;
		std::copy(block, block + blockSize, std::ostream_iterator<std::string>(ss, "\n"));
	}
}
