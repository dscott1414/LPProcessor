package show;
	public class patternElementMatch {
		short begin;
		short end;
		byte __patternElement;
		byte __patternElementIndex;
		int nextByPosition; // the next PEMA entry in the same position - allows
												// routines to traverse downwards
		int nextByPatternEnd; // the next PEMA entry in the same position with the
													// same pattern and end
		// nextByPatternEnd is set negative if it points back to the beginning of
		// the loop
		int nextByChildPatternEnd; // the next PEMA entry in the same position with
																// the same childPattern and childEnd
		int nextPatternElement; // the next PEMA entry in the next position with the
														// same pattern and end - allows routines to skip
														// forward by pattern
		int origin; // beginning of the chain for the first element of the pattern
		// int sourcePosition; // so position not needed on print
		short iCost; // lowest cost of PMA element
		short cumulativeDeltaCost; // accumulates costs or benefits of assessCost
																// (from multiple setSecondaryCosts)
		short tempCost; // used for setSecondaryCosts
		int PEMAElementMatchedSubIndex; // points to a pattern #/end OR a form #
		short pattern;
		byte flags;
		short cost;

		public patternElementMatch(LittleEndianDataInputStream rs) {
			begin = rs.readShort();
			end = rs.readShort();
			__patternElement = rs.readByte();
			__patternElementIndex = rs.readByte();
			rs.readShort(); // skip two bytes for alignment
			nextByPosition = rs.readInteger(); // the next PEMA entry in the same
																					// position - allows routines to
																					// traverse downwards
			nextByPatternEnd = rs.readInteger(); // the next PEMA entry in the same
																						// position with the same pattern
																						// and end
			// nextByPatternEnd is set negative if it points back to the beginning of
			// the loop
			nextByChildPatternEnd = rs.readInteger(); // the next PEMA entry in the
																								// same position with the same
																								// childPattern and childEnd
			nextPatternElement = rs.readInteger(); // the next PEMA entry in the next
																							// position with the same pattern
																							// and end - allows routines to
																							// skip forward by pattern
			origin = rs.readInteger(); // beginning of the chain for the first element
																	// of the pattern
			// int sourcePosition; // so position not needed on print
			iCost = rs.readShort(); // lowest cost of PMA element
			cumulativeDeltaCost = rs.readShort(); // accumulates costs or benefits of
																						// assessCost (from multiple
																						// setSecondaryCosts)
			tempCost = rs.readShort(); // used for setSecondaryCosts
			rs.readShort(); // skip two bytes for alignment
			PEMAElementMatchedSubIndex = rs.readInteger(); // points to a pattern
																											// #/end OR a form #
			pattern = rs.readShort();
			flags = rs.readByte();
			rs.readByte(); // skip byte for alignment
			cost = rs.readShort();
			rs.readShort(); // skip two bytes for alignment
		}
	}

