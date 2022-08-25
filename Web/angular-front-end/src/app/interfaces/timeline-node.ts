export interface TimelineNode {
  speaker: string,
  timelineSegment: any,
  children: TimelineNode[]
  relations: any[]
}
