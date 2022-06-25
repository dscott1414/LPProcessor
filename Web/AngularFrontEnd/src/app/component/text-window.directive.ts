import { Directive, ViewContainerRef } from '@angular/core';

@Directive({
  selector: '[textDirectiveHost]',
})
export class TextWindowDirective {
  constructor(public viewContainerRef: ViewContainerRef) { }
}
