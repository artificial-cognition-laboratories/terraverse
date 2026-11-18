/** Throw from any handler to stop the action. The player is shown the reason. */
export class Refused extends Error {
    constructor(reason: string) {
        super(reason)
        this.name = "Refused"
    }
}
